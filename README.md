# GIF_SD_CARD

# Cách upload file .bin bằng Web ESP Tool
  * B1. Vào web https://espressif.github.io/esptool-js/
  * B2. Chọn connect, chọn port.
  * B3. Làm như ảnh:
  * <img width="910" alt="123" src="https://github.com/user-attachments/assets/5a388ed2-791e-40ff-9d64-205cb50e5089" />
  * B4. Chọn Program để tiến hành nạp.
  * B5. Nhấn nút rst để khởi động lại ESP32.

  * Anh em lưu ý đối với dòng s3 thì Flash address sẽ là:
      - 0X0000    file bootloader
      - 0X8000    file partitions
      - 0X10000   file ino

# Sơ đồ ESP32 S3 SUPER MINI

<img width="430" height="309" alt="s3 supermini gif" src="https://github.com/user-attachments/assets/f9922488-81f1-4511-a0e8-5f1143b17fd1" />

# Sơ đồ ESP32 S3 DEV KIT

<img width="451" height="339" alt="s3 devkit gif" src="https://github.com/user-attachments/assets/78979805-a62c-4dae-968d-3818bd853671" />

# Sơ đồ ESP32 30P TFT 0.96

<img width="445" height="267" alt="TFT 0 96" src="https://github.com/user-attachments/assets/b484c7a0-4d21-4751-b670-2d55f2a14404" />
