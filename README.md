# Daikin_HT_RTDW_modbus_esp8266

Read and write parameters from/to a Daikin Altherma HT with a RTD-W modbus interface via MQTT

RTD-W interface: http://www.realtime-controls.co.uk/index.php/site/multi-language-download/rtd_w_daikin_control_interface

The list of modbus registers is on page 9 and 10 in http://www.realtime-controls.co.uk/rtd/EN%20RTD-W%20Installation%20Instructions%2020670-1.07.pdf

RS485 module: https://www.aliexpress.com/item/2PCS-MAX485-module-RS-485-module-TTL-to-RS-485-module/1893567852.html

RTD-Ws port A and B are reversed, connect as follows:

RTD-W      RS485 module

  A ---------- B
  
  B ---------- A
