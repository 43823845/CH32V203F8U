// Tactical WML HUD - Frontend Logic for Tauri 2
// 支持真实硬件 USB 虚拟串口通信 与 内置虚拟仿真自测模式

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
let isSimulationMode = false;
let rxLineBuffer = "";
let statusPollInterval = null;

// 虚拟仿真测试状态
let simState = {
  state: 1,
  vbat: 3840,
  batTier: 0,
  pwm1: 1000,
  pwm2: 0,
  timeout: 10
};

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
  portSelect.innerHTML = "";

  // 1. 始终提供一个虚拟仿真自测设备选项，供无硬件自测与UI验证
  const simOpt = document.createElement("option");
  simOpt.value = "__SIMULATOR__";
  simOpt.textContent = "🎮 [虚拟仿真设备] Tactical Gunlight Simulator";
  portSelect.appendChild(simOpt);

  let realPortCount = 0;
  let targetSelectIndex = 0;

  if (invoke) {
    try {
      const ports = await invoke("list_ports");
      ports.forEach((p) => {
        realPortCount++;
        const opt = document.createElement("option");
        opt.value = p.port_name;
        opt.textContent = p.description;

        // 自动优先选中 WCH CDC 枪灯设备 (VID: 0x1A86, PID: 0xFE0C)
        if (p.vid === 0x1a86 || (p.product && p.product.includes("WML")) || (p.product && p.product.includes("CDC"))) {
          opt.textContent += " ⭐ [战术枪灯已识别]";
          targetSelectIndex = portSelect.options.length;
        }
        portSelect.appendChild(opt);
      });
    } catch (err) {
      logToTerminal(`[ERR] 扫描物理串口异常: ${err}`, "err-line");
    }
  }

  // 若扫描到了真实枪灯，默认选中真实枪灯；否则默认选中虚拟仿真器
  portSelect.selectedIndex = targetSelectIndex;
}

// 连接/断开串口
async function toggleConnection() {
  if (isConnected) {
    // 断开连接
    if (isSimulationMode) {
      isSimulationMode = false;
      setConnectedState(false);
      logToTerminal("[SYS] 虚拟仿真设备已断开连接", "sys-line");
    } else if (invoke) {
      try {
        await invoke("close_port");
        setConnectedState(false);
        logToTerminal("[SYS] 物理串口已主动断开", "sys-line");
      } catch (err) {
        logToTerminal(`[ERR] 关闭串口失败: ${err}`, "err-line");
      }
    }
  } else {
    // 建立连接
    const portName = portSelect.value;
    const baudRate = parseInt(baudSelect.value, 10);

    if (!portName) {
      alert("请选择有效的设备或仿真模式！");
      return;
    }

    if (portName === "__SIMULATOR__") {
      // 启动虚拟仿真自测
      isSimulationMode = true;
      setConnectedState(true);
      logToTerminal("[SYS] 已连接至【虚拟仿真设备】。所有指令、遥测与UI控件均可直接脱机测试验证！", "sys-line");
      setTimeout(() => {
        handleSimCommand("STATUS");
      }, 100);
      return;
    }

    // 真实硬件串口
    if (!invoke) {
      alert("当前处于浏览器开发环境，请选择【虚拟仿真设备】进行功能自测！");
      return;
    }

    try {
      btnConnect.disabled = true;
      btnConnect.textContent = "CONNECTING...";
      await invoke("open_port", { portName, baudRate });
      isSimulationMode = false;
      setConnectedState(true);
      logToTerminal(`[SYS] 成功连接至物理串口: ${portName} @ ${baudRate} bps`, "sys-line");

      setTimeout(() => {
        sendCommand("STATUS");
      }, 300);
    } catch (err) {
      logToTerminal(`[ERR] 打开物理串口失败: ${err}`, "err-line");
      alert(`无法连接物理串口: ${err}`);
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
    connIndicator.querySelector(".text").textContent = isSimulationMode ? "SIMULATOR ACTIVE" : "CONNECTED";
    portSelect.disabled = true;
    baudSelect.disabled = true;

    // 开启周期性状态同步 (每 3 秒同步一次 STATUS)
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

// 模拟固件响应执行器 (用于无硬件自测)
function handleSimCommand(cmd) {
  const c = cmd.trim();
  const upper = c.toUpperCase();

  if (upper === "STATUS") {
    // 模拟轻微电压波动 (3820 ~ 3850 mV)
    simState.vbat = 3800 + Math.floor(Math.random() * 50);
    const resp = `[STATUS] State: ${simState.state}, Vbat: ${simState.vbat} mV, BatTier: ${simState.batTier}, PWM1: ${simState.pwm1}/1000, PWM2: ${simState.pwm2}/1000, Timeout: ${simState.timeout}s`;
    handleFirmwareLine(resp);
  } else if (upper.startsWith("SET PWM1 ")) {
    const val = Math.max(0, Math.min(1000, parseInt(c.substring(9), 10) || 0));
    simState.pwm1 = val;
    handleFirmwareLine(`[OK] Set PWM1 = ${val}/1000 (${(val / 10).toFixed(1)}%)`);
    handleSimCommand("STATUS");
  } else if (upper.startsWith("SET PWM2 ")) {
    const val = Math.max(0, Math.min(1000, parseInt(c.substring(9), 10) || 0));
    simState.pwm2 = val;
    handleFirmwareLine(`[OK] Set PWM2 = ${val}/1000 (${(val / 10).toFixed(1)}%)`);
    handleSimCommand("STATUS");
  } else if (upper.startsWith("SET MODE ")) {
    const mode = parseInt(c.substring(9), 10);
    if (mode >= 0 && mode <= 5) {
      simState.state = mode;
      if (mode === 0) { simState.pwm1 = 0; simState.pwm2 = 0; }
      else if (mode === 1) { simState.pwm1 = 1000; simState.pwm2 = 0; }
      else if (mode === 2) { simState.pwm1 = 250; simState.pwm2 = 0; }
      else if (mode === 3) { simState.pwm1 = 1000; simState.pwm2 = 1000; }
      else if (mode === 4) { simState.pwm1 = 1000; simState.pwm2 = 0; }
      else if (mode === 5) { simState.pwm1 = 1000; simState.pwm2 = 0; }
      handleFirmwareLine(`[OK] Mode switched to ${mode}`);
      handleSimCommand("STATUS");
    } else {
      handleFirmwareLine("[ERR] Invalid mode 0-5");
    }
  } else if (upper.startsWith("SET TIMEOUT ")) {
    const sec = parseInt(c.substring(12), 10) || 10;
    if (sec >= 1 && sec <= 120) {
      simState.timeout = sec;
      handleFirmwareLine(`[OK] Set Standby Timeout = ${sec}s`);
    } else {
      handleFirmwareLine("[ERR] Timeout range: 1~120s");
    }
  } else if (upper === "ISP") {
    handleFirmwareLine("[SYS] Jump to Bootloader requested via USB...");
    handleFirmwareLine("[SYS] Device disconnected and entering ISP mode.");
  } else if (upper === "REBOOT") {
    handleFirmwareLine("[SYS] Rebooting MCU...");
    setTimeout(() => {
      handleFirmwareLine("[SYS] Boot: CH32V203 Tactical Gunlight V3.0 Ready.");
    }, 500);
  } else if (upper === "HELP") {
    handleFirmwareLine("\r\n=== Tactical Gunlight CLI Commands ===");
    handleFirmwareLine("  STATUS              : Get all telemetry");
    handleFirmwareLine("  SET PWM1 <0-1000>   : Set Main WLED duty");
    handleFirmwareLine("  SET PWM2 <0-1000>   : Set Laser duty");
    handleFirmwareLine("  SET MODE <0-5>      : Set state (0:Off, 1:100%, 2:25%, 3:Dual, 4:Strobe, 5:SOS)");
    handleFirmwareLine("  SET TIMEOUT <1-120> : Set auto-sleep seconds");
    handleFirmwareLine("  ISP                 : Jump to factory Bootloader");
    handleFirmwareLine("  REBOOT              : Soft reset system\r\n");
  } else {
    handleFirmwareLine(`[ERR] Unknown command: ${c}. Type HELP.`);
  }
}

// 发送指令
async function sendCommand(cmd, logTx = true) {
  if (!isConnected) {
    if (logTx) logToTerminal(`[WARN] 未连接设备，指令 [${cmd}] 未发送`, "err-line");
    return;
  }

  if (logTx) {
    logToTerminal(`> ${cmd}`, "tx-line");
  }

  if (isSimulationMode) {
    setTimeout(() => handleSimCommand(cmd), 50);
    return;
  }

  if (invoke) {
    try {
      await invoke("send_command", { cmd });
    } catch (err) {
      logToTerminal(`[ERR] 发送失败: ${err}`, "err-line");
    }
  }
}

// 解析从固件接收到的文本行
function handleFirmwareLine(line) {
  const cleanLine = line.trim();
  if (!cleanLine) return;

  logToTerminal(cleanLine, "rx-line");

  if (cleanLine.includes("[STATUS]")) {
    parseStatusLine(cleanLine);
  } else if (cleanLine.includes("[PWR] Battery Tier Switch")) {
    sendCommand("STATUS", false);
  }
}

function parseStatusLine(line) {
  try {
    // 1. 工作状态
    const stateMatch = line.match(/State:\s*(\d+)/i);
    if (stateMatch) {
      const stateId = parseInt(stateMatch[1], 10);
      const stateNames = [
        "0 (ALL OFF 待机休眠)",
        "1 (MODE 1: 100% 满功率)",
        "2 (MODE 2: 25% 节能)",
        "3 (MODE 3: 主灯+激光双开)",
        "4 (STROBE: 10Hz 爆闪)",
        "5 (SOS: 求救莫尔斯)"
      ];
      badgeMode.textContent = stateNames[stateId] || `模式 (${stateId})`;
    }

    // 2. 电池电压
    const vbatMatch = line.match(/Vbat:\s*(\d+)\s*mV/i);
    if (vbatMatch) {
      const vbat = parseInt(vbatMatch[1], 10);
      valVbat.innerHTML = `${vbat} <span class="unit">mV</span>`;

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

    // 3. 电池阶梯
    const tierMatch = line.match(/BatTier:\s*(\d+)/i);
    if (tierMatch) {
      const tierId = parseInt(tierMatch[1], 10);
      const tierNames = [
        "NORMAL (满血 >=3.4V)",
        "LOW (节能降额 25% 3.1~3.4V)",
        "CRITICAL (保命微光 5% 2.95~3.1V)",
        "CUTOFF (截止休眠 <2.95V)"
      ];
      valBatTier.textContent = `阶梯: ${tierNames[tierId] || tierId}`;
    }

    // 4. PWM1 主灯
    const pwm1Match = line.match(/PWM1:\s*(\d+)/i);
    if (pwm1Match) {
      const p1 = parseInt(pwm1Match[1], 10);
      const p1Pct = (p1 / 10).toFixed(1);
      valPwm1.innerHTML = `${p1} <span class="unit">‰ (${p1Pct}%)</span>`;
      barPwm1.style.width = `${p1 / 10}%`;
      sliderPwm1.value = p1;
      textSliderPwm1.textContent = `${p1} ‰`;
    }

    // 5. PWM2 瞄准激光
    const pwm2Match = line.match(/PWM2:\s*(\d+)/i);
    if (pwm2Match) {
      const p2 = parseInt(pwm2Match[1], 10);
      const p2Pct = (p2 / 10).toFixed(1);
      valPwm2.innerHTML = `${p2} <span class="unit">‰ (${p2Pct}%)</span>`;
      barPwm2.style.width = `${p2 / 10}%`;
      sliderPwm2.value = p2;
      textSliderPwm2.textContent = `${p2} ‰`;
    }

    // 6. 超时时间
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

// 初始化事件绑定
function setupEvents() {
  btnRefresh.addEventListener("click", refreshPorts);
  btnConnect.addEventListener("click", toggleConnection);

  btnQueryStatus.addEventListener("click", () => sendCommand("STATUS"));

  // 模式切换按钮组
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
    const ok = confirm("确认发送 ISP 指令让枪灯跳转至 Bootloader 刷机模式吗？\r\n\r\n跳转后串口将断开，直接使用 WCHISPTool 即可刷写新固件！");
    if (ok) {
      sendCommand("ISP");
      logToTerminal("[SYS] 已发送 ISP 指令！设备正在切换至 Bootloader...", "sys-line");
    }
  });

  // 重启
  btnReboot.addEventListener("click", () => {
    sendCommand("REBOOT");
  });

  // 帮助
  btnHelp.addEventListener("click", () => {
    sendCommand("HELP");
  });

  // 终端命令行交互
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

  // 清屏
  btnClearTerm.addEventListener("click", () => {
    termScreen.innerHTML = "";
  });

  // Tauri 事件监听
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

// 页面自启动
window.addEventListener("DOMContentLoaded", () => {
  setupEvents();
  refreshPorts();
});
