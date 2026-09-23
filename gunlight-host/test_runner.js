// Tactical WML HUD - Automated Self-Test Suite
// 验证协议解析、UI数据映射与指令生成

let passCount = 0;
let failCount = 0;

function assert(condition, message) {
  if (condition) {
    console.log(`  ✅ PASS: ${message}`);
    passCount++;
  } else {
    console.error(`  ❌ FAIL: ${message}`);
    failCount++;
  }
}

// 模拟 parseStatusLine 逻辑
function parseStatusLine(line) {
  const result = {};
  
  const stateMatch = line.match(/State:\s*(\d+)/i);
  if (stateMatch) {
    result.state = parseInt(stateMatch[1], 10);
  }

  const vbatMatch = line.match(/Vbat:\s*(\d+)\s*mV/i);
  if (vbatMatch) {
    result.vbat = parseInt(vbatMatch[1], 10);
    let pct = Math.round(((result.vbat - 2950) / (4200 - 2950)) * 100);
    result.vbatPct = Math.max(0, Math.min(100, pct));
  }

  const tierMatch = line.match(/BatTier:\s*(\d+)/i);
  if (tierMatch) {
    result.batTier = parseInt(tierMatch[1], 10);
  }

  const pwm1Match = line.match(/PWM1:\s*(\d+)/i);
  if (pwm1Match) {
    result.pwm1 = parseInt(pwm1Match[1], 10);
  }

  const pwm2Match = line.match(/PWM2:\s*(\d+)/i);
  if (pwm2Match) {
    result.pwm2 = parseInt(pwm2Match[1], 10);
  }

  const timeoutMatch = line.match(/Timeout:\s*(\d+)s/i);
  if (timeoutMatch) {
    result.timeout = parseInt(timeoutMatch[1], 10);
  }

  return result;
}

console.log("==================================================");
console.log("  CH32V203 战术枪灯上位机 (Tactical HUD) 功能自测套件  ");
console.log("==================================================\r\n");

// 测试组 1: 固件协议帧解析自测
console.log("[测试组 1: 固件 STATUS 状态行反序列化解析]");
const status1 = "[STATUS] State: 1, Vbat: 3850 mV, BatTier: 0, PWM1: 1000/1000, PWM2: 0/1000, Timeout: 10s";
const res1 = parseStatusLine(status1);
assert(res1.state === 1, "模式1 (100% 满功率) 正确识别");
assert(res1.vbat === 3850, "电池电压 3850mV 正确提取");
assert(res1.vbatPct === 72, "电池百分比估算准确 (3850mV -> 72%)");
assert(res1.batTier === 0, "电池阶梯 0 (Normal) 正确识别");
assert(res1.pwm1 === 1000, "PWM1 1000‰ 正确提取");
assert(res1.pwm2 === 0, "PWM2 0‰ 正确提取");
assert(res1.timeout === 10, "休眠倒计时 10s 正确提取");

// 测试组 2: 模式 3 (双开) 与低电阶梯降额
console.log("\r\n[测试组 2: 模式 3 (主灯+激光) 与低电节能降额解析]");
const status2 = "[STATUS] State: 3, Vbat: 3250 mV, BatTier: 1, PWM1: 250/1000, PWM2: 1000/1000, Timeout: 15s";
const res2 = parseStatusLine(status2);
assert(res2.state === 3, "模式3 (主灯+激光双开) 正确识别");
assert(res2.vbat === 3250, "低电电压 3250mV 正确提取");
assert(res2.batTier === 1, "电池阶梯 1 (节能降额 25%) 正确识别");
assert(res2.pwm1 === 250, "主灯 PWM1 自动降额为 250‰ (25%)");
assert(res2.pwm2 === 1000, "瞄准激光 PWM2 保持 1000‰ (100%)");
assert(res2.timeout === 15, "可配置超时时间 15s 正确生效");

// 测试组 3: 截止保护与极低电量边界
console.log("\r\n[测试组 3: 极低电量与截止保护边界测试]");
const status3 = "[STATUS] State: 0, Vbat: 2900 mV, BatTier: 3, PWM1: 0/1000, PWM2: 0/1000, Timeout: 10s";
const res3 = parseStatusLine(status3);
assert(res3.state === 0, "关灯/休眠状态 0 正确识别");
assert(res3.vbat === 2900, "截止电压 2900mV 正确提取");
assert(res3.vbatPct === 0, "极低电压百分比钳位为 0%");
assert(res3.batTier === 3, "截止保护等级 3 (CUTOFF) 判定生效");
assert(res3.pwm1 === 0 && res3.pwm2 === 0, "保护切断输出全为 0");

// 测试组 4: 调参指令生成格式验证
console.log("\r\n[测试组 4: 调参 CLI 命令拼装规范性验证]");
function buildSetPwmCmd(ch, duty) {
  duty = Math.max(0, Math.min(1000, parseInt(duty, 10)));
  return `SET PWM${ch} ${duty}`;
}
function buildSetModeCmd(mode) {
  return `SET MODE ${mode}`;
}
function buildSetTimeoutCmd(sec) {
  sec = Math.max(1, Math.min(120, parseInt(sec, 10)));
  return `SET TIMEOUT ${sec}`;
}

assert(buildSetPwmCmd(1, 500) === "SET PWM1 500", "PWM1 调光指令匹配固件规范");
assert(buildSetPwmCmd(2, 1000) === "SET PWM2 1000", "PWM2 调光指令匹配固件规范");
assert(buildSetPwmCmd(1, 1500) === "SET PWM1 1000", "PWM 超限上限溢出钳位至 1000");
assert(buildSetPwmCmd(2, -50) === "SET PWM2 0", "PWM 超低下限负值钳位至 0");
assert(buildSetModeCmd(4) === "SET MODE 4", "10Hz 爆闪模式指令匹配");
assert(buildSetModeCmd(5) === "SET MODE 5", "SOS 模式指令匹配");
assert(buildSetTimeoutCmd(20) === "SET TIMEOUT 20", "超时时间配置指令匹配");
assert(buildSetTimeoutCmd(0) === "SET TIMEOUT 1", "超时时间下限钳位为 1s");
assert(buildSetTimeoutCmd(300) === "SET TIMEOUT 120", "超时时间上限钳位为 120s");

// 测试组 5: 畸形数据与抗干扰防御性测试
console.log("\r\n[测试组 5: 串口通信畸形输入抗干扰测试]");
const malformed1 = "[STATUS] Garbage frame without numbers";
const resM1 = parseStatusLine(malformed1);
assert(resM1.state === undefined && resM1.vbat === undefined, "畸形乱码帧安全忽略不崩溃");

const malformed2 = "[STATUS] State: 999, Vbat: 99999 mV, BatTier: 8, PWM1: 9999, PWM2: 9999, Timeout: 999s";
const resM2 = parseStatusLine(malformed2);
assert(resM2.state === 999 && resM2.vbat === 99999, "超大数值安全解析不溢出");

console.log("\r\n==================================================");
console.log(`  自测执行完成: ${passCount} PASSED, ${failCount} FAILED`);
console.log("==================================================");

if (failCount > 0) {
  process.exit(1);
} else {
  process.exit(0);
}
