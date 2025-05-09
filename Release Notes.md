SCM1612 SDK Release Notes - Version [V1.0.0]
===========================================

Release Date: August 29th, 2023

The SDK is designed to support Senscomm SCM1612: a 1T1R 2.4GHz Wi-Fi 6 + BLE 5.1 SoC solution with an integrated MCU.

For any issues, suggestions, or feedback encountered while using the SCM1612, please reach out to our support team at: [support@senscomm.com]

We hope you have a pleasant experience using our SDK!

Best regards,
[Senscomm/IoT Team]

### Known Issues:
- DUT may hang during stress tests, with invalid IP pings, or after "filter_del" command.
- Occasional garbled console messages.
- Some stations struggle to connect with SoftAP.
- Connection issues from WPA3 to WPA2/WPA.
- "Hidden SSID" not support yet.
- SAP_CH13 not support yet.
- Throughput in cable mode may get lower than Over-The-Air (OTA)
- Adjusting beacon interval might make SAP undetectable
- "wifi reg_evt_cb" can cause a DUT assertion.
- No return value from "wifi sta_scan" when DUT is connected.