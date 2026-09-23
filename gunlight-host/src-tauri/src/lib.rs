use std::io::{Read, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct PortDesc {
    pub port_name: String,
    pub description: String,
    pub is_usb: bool,
    pub vid: Option<u16>,
    pub pid: Option<u16>,
    pub manufacturer: Option<String>,
    pub product: Option<String>,
}

struct SerialState {
    port: Option<Box<dyn serialport::SerialPort>>,
    running: Arc<AtomicBool>,
    active_port_name: Option<String>,
}

#[derive(Clone, Serialize)]
struct SerialRxPayload {
    data: String,
}

#[tauri::command]
fn list_ports() -> Result<Vec<PortDesc>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    let mut list = Vec::new();

    for p in ports {
        let (is_usb, vid, pid, mfr, prod) = match &p.port_type {
            serialport::SerialPortType::UsbPort(info) => (
                true,
                Some(info.vid),
                Some(info.pid),
                info.manufacturer.clone(),
                info.product.clone(),
            ),
            _ => (false, None, None, None, None),
        };

        let desc = if let Some(ref prod_name) = prod {
            format!("{} ({})", p.port_name, prod_name)
        } else {
            p.port_name.clone()
        };

        list.push(PortDesc {
            port_name: p.port_name,
            description: desc,
            is_usb,
            vid,
            pid,
            manufacturer: mfr,
            product: prod,
        });
    }

    Ok(list)
}

#[tauri::command]
fn open_port(
    app: AppHandle,
    state: tauri::State<Arc<Mutex<SerialState>>>,
    port_name: String,
    baud_rate: u32,
) -> Result<(), String> {
    let mut st = state.lock();

    // 先停止已有连接
    st.running.store(false, Ordering::Relaxed);
    st.port = None;
    st.active_port_name = None;

    let port = serialport::new(&port_name, baud_rate)
        .timeout(Duration::from_millis(50))
        .open()
        .map_err(|e| format!("无法打开串口 {}: {}", port_name, e))?;

    let running = Arc::new(AtomicBool::new(true));
    st.running = running.clone();
    st.active_port_name = Some(port_name.clone());

    // 克隆用于读取线程
    let mut reader_port = port.try_clone().map_err(|e| e.to_string())?;
    st.port = Some(port);

    let app_handle = app.clone();
    let port_label = port_name.clone();

    std::thread::spawn(move || {
        let mut buf = [0u8; 1024];
        while running.load(Ordering::Relaxed) {
            match reader_port.read(&mut buf) {
                Ok(n) if n > 0 => {
                    let s = String::from_utf8_lossy(&buf[..n]).to_string();
                    let _ = app_handle.emit("serial-rx", SerialRxPayload { data: s });
                }
                Ok(_) => {
                    std::thread::sleep(Duration::from_millis(5));
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                    // 超时属于正常现象
                }
                Err(_) => {
                    // 硬件物理断开连接
                    let _ = app_handle.emit("serial-error", format!("端口 {} 已断开连接", port_label));
                    break;
                }
            }
        }
    });

    Ok(())
}

#[tauri::command]
fn close_port(state: tauri::State<Arc<Mutex<SerialState>>>) -> Result<(), String> {
    let mut st = state.lock();
    st.running.store(false, Ordering::Relaxed);
    st.port = None;
    st.active_port_name = None;
    Ok(())
}

#[tauri::command]
fn send_command(
    state: tauri::State<Arc<Mutex<SerialState>>>,
    cmd: String,
) -> Result<(), String> {
    let mut st = state.lock();
    if let Some(ref mut port) = st.port {
        let mut data = cmd.into_bytes();
        if !data.ends_with(b"\n") {
            data.extend_from_slice(b"\r\n");
        }
        port.write_all(&data).map_err(|e| format!("发送失败: {}", e))?;
        port.flush().map_err(|e| format!("刷新串口失败: {}", e))?;
        Ok(())
    } else {
        Err("串口未连接".to_string())
    }
}

#[tauri::command]
fn get_connection_status(state: tauri::State<Arc<Mutex<SerialState>>>) -> Result<Option<String>, String> {
    let st = state.lock();
    Ok(st.active_port_name.clone())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let initial_state = Arc::new(Mutex::new(SerialState {
        port: None,
        running: Arc::new(AtomicBool::new(false)),
        active_port_name: None,
    }));

    tauri::Builder::default()
        .manage(initial_state)
        .invoke_handler(tauri::generate_handler![
            list_ports,
            open_port,
            close_port,
            send_command,
            get_connection_status
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
