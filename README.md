A Weather Forecast Clock 
-----------------------
Introduction 
------------------------
This is a device using ESP32 microcontroller. It has some function of a multi - funtions clock including:
* Show a real-time such as day time
* A weather forecast station
* A device can monitor temperature, humidity and gas, able to show them on OLED
* Able to measure heart rate and blood oxygen
* A analog clock for decoration

Knowledge used in the project 
--------------------------------
* Use I2C communication to connect OLED, Max30100
* Use API by HTTP method to receive current time and weather information
* Use Google Firebase to store data when transmiting it
* Use Ledc to creat PWM so that Buzzer can play
_________________________
Principle diagram
----------------------------
![Sơ đồ nguyên lý](https://github.com/user-attachments/assets/82937f65-2d9b-495c-9282-11b71c35ebc4)

--------------------------
Result
--------------------------
* Show current time
  ![Picture1](https://github.com/user-attachments/assets/cc4f3cac-4e30-4208-9edb-d80d52e0f526)
* Show current weather outside
![Picture4](https://github.com/user-attachments/assets/d12bf874-2aba-490f-b29b-4acddbd61de9)
* Show heart rate and blood oxygen
![Picture5](https://github.com/user-attachments/assets/e4c58ba2-239a-45ef-8a03-ddeb3e2dad85)
* Show tempurature, humidity and Buzzer plays when exits toxic gas
  ![Picture6](https://github.com/user-attachments/assets/45c34bd1-7a7f-4900-aad4-576900576ef9)
* A analog clock
  ![Picture7](https://github.com/user-attachments/assets/89077698-4340-4f81-bd16-e3cf47e2c2d3)







