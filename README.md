# M5paper_weather_report
Weather reporting tool for M5paper


# M5 Library bug
* pio/libdeps/m5stack-fire/M5EPD/src/utility/BM8563.cpp
commentout a initialize in function of BM8563::setAlarmIRQ(const rtc_time_t &time);
```
    //out_buf[2] = 0x00;
    //out_buf[3] = 0x00;
```
