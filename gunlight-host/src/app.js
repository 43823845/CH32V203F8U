// Tactical WML HUD - Frontend Logic for Tauri 2

const invoke = window.__TAURI__ ? window.__TAURI__.core.invoke : null;
const event = window.__TAURI__ ? window.__TAURI__.event : null;

// UI Elements
const portSelect = document.getElementById("port-select");
const baudSelect = document.getElementById("baud-select");
const btnRefresh = document.getElementById("btn-refresh");
const btnConnect = document.getElementById("btn-connect");
const connIndicator = document.getElementById("conn-indicator");

const btnQueryStatus = document.getElementById("btn-query-status");
const valVbat = document.getElementById("val-vbat");
const barVbat = document.getElementById("bar-vbat");
const valBatTier = document.getElementById("val-bat-tier");
const badgeMode = document.getElementById("badge-mode");
const valTimeout = document.getElementById("val-timeout");
const valPwm1 = document.getElementById("val-pwm1");
const barPwm1 = document.getElementById("bar-pwm1");
const valPwm2 = document.getElementById("val-pwm2");
const barPwm2 = document.getElementById("bar-pwm2");

const sliderPwm1 = document.getElementById("slider-pwm1");
const textSliderPwm1 = document.getElementById("text-slider-pwm1");
const sliderPwm2 = document.getElementById("slider-pwm2");
const textSliderPwm2 = document.getElementById("text-slider-pwm2");
const inputTimeout = document.getElementById("input-timeout");
const btnSetTimeout = document.getElementById("btn-set-timeout");

const btnIsp = document.getElementById("btn-isp");
const btnReboot = document.getElementById("btn-reboot");
const btnHelp = document.getElementById("btn-help");

const termScreen = document.getElementById("term-screen");
const termInput = document.getElementById("term-input");
const btnSendCmd = document.getElementById("btn-send-cmd");
const btnClearTerm = document.getElementById("btn-clear-term");
const chkAutoscroll = document.getElementById("chk-autoscroll");

let isConnected = false;
let rxLineBuffer = "";
let statusPollInterval = null;

// 日志输出辅助
function logToTerminal(text, type = "rx-line") {
  const line = document.createElement("div");
  line.className = `log-line ${type}`;
  line.textContent = text;
  termScreen.appendChild(line);

  if (chkAutoscroll.checked) {
    termScreen.scrollTop = termScreen.scrollHeight;
  }
}

// 刷新串口列表
async function refreshPorts() {
  if (!invoke) {
    logToTerminal("[WARN] Tauri invoke API unavailable. Running in browser mode?", "err-line");
    return;
  }

  try {
    const ports = await invoke("list_ports");
    portSelect.innerHTML = "";

    if (ports.length === 0) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "未发现可用串口";
      portSelect.appendChild(opt);
      return;
    }

    let defaultSelectIndex = 0;
    ports.forEach((p, idx) => {
      const opt = document.createElement("option");
      opt.value = p.port_name;
      opt.textContent = p.description;

      // 自动优先匹配 WCH CDC 枪灯设备 (VID: 0x1A86, PID: 0xFE0C)
      if (p.vid === 0x1a86 || (p.product && p.product.includes("WML"))) {
        opt.textContent += " ⭐ [战术枪灯]";
        defaultSelectIndex = idx;
      }
      portSelect.appendChild(opt);
    });

    portSelect.selectedIndex = defaultSelectIndex;
  } catch (err) {
    logToTerminal(`[ERR] 扫描串口失败: ${err}`, "err-line");
  }
}

// 连接/断开串口
async function toggleConnection() {
  if (!invoke) return;

  if (isConnected) {
    // 断开连接
    try {
      await invoke("close_port");
      setConnectedState(false);
      logToTerminal("[SYS] 串口已主动断开", "sys-line");
    } catch (err) {
      logToTerminal(`[ERR] 关闭串口失败: ${err}`, "err-line");
    }
  } else {
    // 建立连接
    const portName = portSelect.value;
    const baudRate = parseInt(baudSelect.value, 10);

    if (!portName) {
      alert("请选择有效的串口！");
      return;
    }

    try {
      btnConnect.disabled = true;
      btnConnect.textContent = "CONNECTING...";
      await invoke("open_port", { portName, baudRate });
      setConnectedState(true);
      logToTerminal(`[SYS] 成功连接至串口: ${portName} @ ${baudRate} bps`, "sys-line");

      // 连接成功后自动发送一次 STATUS 指令同步仪表盘
      setTimeout(() => {
        sendCommand("STATUS");
      }, 300);
    } catch (err) {
      logToTerminal(`[ERR] 打开串口失败: ${err}`, "err-line");
      alert(`无法连接串口: ${err}`);
      setConnectedState(false);
    } finally {
      btnConnect.disabled = false;
    }
  }
}

function setConnectedState(connected) {
  isConnected = connected;
  if (connected) {
    btnConnect.textContent = "DISCONNECT";
    btnConnect.className = "btn btn-danger";
    connIndicator.className = "conn-status connected";
    connIndicator.querySelector(".text").textContent = "CONNECTED";
    portSelect.disabled = true;
    baudSelect.disabled = true;

    // 开启周期性状态同步 (每 3 秒查询一次 STATUS)
    if (!statusPollInterval) {
      statusPollInterval = setInterval(() => {
        if (isConnected) {
          sendCommand("STATUS", false);
        }
      }, 3000);
    }
  } else {
    btnConnect.textContent = "CONNECT";
    btnConnect.className = "btn btn-primary";
    connIndicator.className = "conn-status";
    connIndicator.querySelector(".text").textContent = "DISCONNECTED";
    portSelect.disabled = false;
    baudSelect.disabled = false;

    if (statusPollInterval) {
      clearInterval(statusPollInterval);
      statusPollInterval = null;
    }
  }
}

// 发送指令
async function sendCommand(cmd, logTx = true) {
  if (!isConnected || !invoke) {
    if (logTx) logToTerminal(`[WARN] 未连接串口，指令 [${cmd}] 未发送`, "err-line");
    return;
  }

  try {
    if (logTx) {
      logToTerminal(`> ${cmd}`, "tx-line");
    }
    await invoke("send_command", { cmd });
  } catch (err) {
    logToTerminal(`[ERR] 发送失败: ${err}`, "err-line");
  }
}

// 解析从固件接收到的文本行
function handleFirmwareLine(line) {
  const cleanLine = line.trim();
  if (!cleanLine) return;

  logToTerminal(cleanLine, "rx-line");

  // 1. 尝试解析 [STATUS] 行:
  // 例如: [STATUS] State: 1, Vbat: 3850 mV, BatTier: 0, PWM1: 1000/1000, PWM2: 0/1000, Timeout: 10s
  if (cleanLine.includes("[STATUS]")) {
    parseStatusLine(cleanLine);
  } else if (cleanLine.includes("[PWR] Battery Tier Switch")) {
    // 电池阶梯变更提示，自动查询状态
    sendCommand("STATUS", false);
  }
}

function parseStatusLine(line) {
  try {
    // State
    const stateMatch = line.match(/State:\s*(\d+)/i);
    if (stateMatch) {
      const stateId = parseInt(stateMatch[1], 10);
      const stateNames = [
        "0 (ALL OFF 待机)",
        "1 (MODE 1: 100% 满功率)",
        "2 (MODE 2: 25% 节能)",
        "3 (MODE 3: 主灯+激光双开)",
        "4 (STROBE: 10Hz 爆闪)",
        "5 (SOS: 求救莫尔斯)"
      ];
      badgeMode.textContent = stateNames[stateId] || `未知模式 (${stateId})`;
    }

    // Vbat
    const vbatMatch = line.match(/Vbat:\s*(\d+)\s*mV/i);
    if (vbatMatch) {
      const vbat = parseInt(vbatMatch[1], 10);
      valVbat.innerHTML = `${vbat} <span class="unit">mV</span>`;

      // 锂电池电压百分比估算 (2.95V ~ 4.2V)
      let pct = Math.round(((vbat - 2950) / (4200 - 2950)) * 100);
      pct = Math.max(0, Math.min(100, pct));
      barVbat.style.width = `${pct}%`;

      if (vbat >= 3450) {
        barVbat.className = "progress-bar-fill";
      } else if (vbat >= 3100) {
        barVbat.className = "progress-bar-fill fill-amber";
      } else {
        barVbat.className = "progress-bar-fill fill-red";
      }
    }

    // BatTier
    const tierMatch = line.match(/BatTier:\s*(\d+)/i);
    if (tierMatch) {
      const tierId = parseInt(tierMatch[1], 10);
      const tierNames = [
        "NORMAL (满血输出 >= 3.4V)",
        "LOW (节能降额 25% 3.1V~3.4V)",
        "CRITICAL (保命月光 5% 2.95V~3.1V)",
        "CUTOFF (截止休眠 < 2.95V)"
      ];
      valBatTier.textContent = `等级: ${tierNames[tierId] || tierId}`;
    }

    // PWM1
    const pwm1Match = line.match(/PWM1:\s*(\d+)/i);
    if (pwm1Match) {
      const p1 = parseInt(pwm1Match[1], 10);
      const p1Pct = (p1 / 10).toFixed(1);
      valPwm1.innerHTML = `${p1} <span class="unit">‰ (${p1Pct}%)</span>`;
      barPwm1.style.width = `${p1 / 10}%`;
      sliderPwm1.value = p1;
      textSliderPwm1.textContent = `${p1} ‰`;
    }

    // PWM2
    const pwm2Match = line.match(/PWM2:\s*(\d+)/i);
    if (pwm2Match) {
      const p2 = parseInt(pwm2Match[1], 10);
      const p2Pct = (p2 / 10).toFixed(1);
      valPwm2.innerHTML = `${p2} <span class="unit">‰ (${p2Pct}%)</span>`;
      barPwm2.style.width = `${p2 / 10}%`;
      sliderPwm2.value = p2;
      textSliderPwm2.textContent = `${p2} ‰`;
    }

    // Timeout
    const timeoutMatch = line.match(/Timeout:\s*(\d+)s/i);
    if (timeoutMatch) {
      const sec = parseInt(timeoutMatch[1], 10);
      valTimeout.textContent = `休眠倒计时: ${sec}s`;
      inputTimeout.value = sec;
    }
  } catch (err) {
    console.error("解析状态行失败:", err);
  }
}

// 接收串口流数据缓冲
function onSerialChunk(data) {
  rxLineBuffer += data;
  let newlineIdx;
  while ((newlineIdx = rxLineBuffer.indexOf("\n")) !== -1) {
    const line = rxLineBuffer.substring(0, newlineIdx);
    rxLineBuffer = rxLineBuffer.substring(newlineIdx + 1);
    handleFirmwareLine(line);
  }
}

// 初始化事件监听
function setupEvents() {
  btnRefresh.addEventListener("click", refreshPorts);
  btnConnect.addEventListener("click", toggleConnection);

  btnQueryStatus.addEventListener("click", () => sendCommand("STATUS"));

  // 模式按钮
  document.querySelectorAll(".btn-mode").forEach((btn) => {
    btn.addEventListener("click", () => {
      const mode = btn.getAttribute("data-mode");
      sendCommand(`SET MODE ${mode}`);
      setTimeout(() => sendCommand("STATUS", false), 200);
    });
  });

  // PWM1 滑块
  sliderPwm1.addEventListener("input", (e) => {
    textSliderPwm1.textContent = `${e.target.value} ‰`;
  });
  sliderPwm1.addEventListener("change", (e) => {
    sendCommand(`SET PWM1 ${e.target.value}`);
    setTimeout(() => sendCommand("STATUS", false), 150);
  });

  // PWM2 滑块
  sliderPwm2.addEventListener("input", (e) => {
    textSliderPwm2.textContent = `${e.target.value} ‰`;
  });
  sliderPwm2.addEventListener("change", (e) => {
    sendCommand(`SET PWM2 ${e.target.value}`);
    setTimeout(() => sendCommand("STATUS", false), 150);
  });

  // 超时设定
  btnSetTimeout.addEventListener("click", () => {
    const sec = parseInt(inputTimeout.value, 10);
    if (isNaN(sec) || sec < 1 || sec > 120) {
      alert("请输入 1~120 之间的超时秒数！");
      return;
    }
    sendCommand(`SET TIMEOUT ${sec}`);
    setTimeout(() => sendCommand("STATUS", false), 150);
  });

  // 一键 ISP 刷机
  btnIsp.addEventListener("click", () => {
    const ok = confirm("确认通过 USB 发送 ISP 指令让枪灯跳转至 Bootloader 刷机模式吗？\r\n\r\n跳转后串口将临时断开，可直接使用 WCHISPTool 或 py-ch32v-isp 烧录新固件！");
    if (ok) {
      sendCommand("ISP");
      logToTerminal("[SYS] 已发送 ISP 指令！设备正在切换至 Bootloader...", "sys-line");
    }
  });

  // 重启
  btnReboot.addEventListener("click", () => {
    sendCommand("REBOOT");
  });

  // CLI 帮助
  btnHelp.addEventListener("click", () => {
    sendCommand("HELP");
  });

  // 终端命令行输入
  const doSendCmd = () => {
    const text = termInput.value.trim();
    if (!text) return;
    sendCommand(text);
    termInput.value = "";
  };
  btnSendCmd.addEventListener("click", doSendCmd);
  termInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") doSendCmd();
  });

  // 清空终端
  btnClearTerm.addEventListener("click", () => {
    termScreen.innerHTML = "";
  });

  // 注册 Tauri 后端事件
  if (event) {
    event.listen("serial-rx", (e) => {
      if (e.payload && e.payload.data) {
        onSerialChunk(e.payload.data);
      }
    });

    event.listen("serial-error", (e) => {
      logToTerminal(`[ERR] ${e.payload}`, "err-line");
      setConnectedState(false);
    });
  }
}

// 页面加载完成后自动初始化
window.addEventListener("DOMContentLoaded", () => {
  setupEvents();
  refreshPorts();
});
