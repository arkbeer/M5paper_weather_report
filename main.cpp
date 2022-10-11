#include<iostream>
#include<vector>
#include <M5EPD.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
M5EPD_Canvas canvas(&M5.EPD);
const int display_width=960, w_interval=20;
const int display_height=540, h_interval=40;
const int block_width=(display_width-w_interval*2)/4;
const int block_height=(display_height-w_interval-h_interval*2)/2;

float getBatteryRaw(){
	uint32_t vol = M5.getBatteryVoltage();
    if (vol < 3300) {
        vol = 3300;
    } else if (vol > 4350) {
        vol = 4350;
    }
    float battery = (float)(vol - 3300) / (float)(4350 - 3300);
    if (battery <= 0.01) {
        battery = 0.01;
    }
    if (battery > 1) {
        battery = 1;
    }
	return battery;	
}
String getBatteryPercentage(){
	char buf[20];
	float battery=getBatteryRaw();
    //uint8_t px = battery * 25;
    sprintf(buf, "%d%%", (int)(battery * 100));
	return String(buf);
}
void setRTCTimeFromNtp(){
		//time set
		configTime(3600*9, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
		int retry_count=0;
		while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry_count<100) {
    		Serial.print(".");
    		delay(100); // 0.1秒毎にリトライ
			++retry_count;
			if(retry_count>100){
				M5.shutdown(60);
			}
  		}
		rtc_time_t RTCtime;
		rtc_date_t RTCDate;
		struct tm tm;
	  	getLocalTime(&tm);

	  	RTCtime.hour = tm.tm_hour;
	  	RTCtime.min = tm.tm_min;
		RTCtime.sec = tm.tm_sec;
	  	M5.RTC.setTime(&RTCtime);

	  	RTCDate.year = tm.tm_year + 1900;
	  	RTCDate.mon = tm.tm_mon + 1;
	  	RTCDate.day = tm.tm_mday;
		RTCDate.week = tm.tm_wday;
	  	M5.RTC.setDate(&RTCDate);
}
String getRTCTimeWithFormat(){
	rtc_time_t RTCtime;
	rtc_date_t RTCDate;
	M5.RTC.getTime(&RTCtime);
	M5.RTC.getDate(&RTCDate);
	const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
	char timeStrbuff[64];
	sprintf(timeStrbuff, "%d/%02d/%02d (%s) %02d:%02d:%02d\n",
                RTCDate.year, RTCDate.mon, RTCDate.day,wd[RTCDate.week],
                RTCtime.hour, RTCtime.min, RTCtime.sec);
	return String(timeStrbuff);
}
String getRTCDay(){
	rtc_date_t RTCDate;
	M5.RTC.getDate(&RTCDate);
	if(RTCDate.day<10){
		return "0"+String(RTCDate.day);
	}
	return String(RTCDate.day);
}


class PrefectureBase{
public:
	const String url;
	HTTPClient http;
	String payload;
	String date;
	String forecast;
	String weather_code;
	std::vector<String> pop_dates;
	std::vector<String> pops;
	String temp_date;
	String temp_min;
	String temp_max;


	PrefectureBase(const String _url):url(_url){
		http.begin(url);
		int httpCode = http.GET();
		if (httpCode > 0){
  			if (httpCode == HTTP_CODE_OK){
    			payload = http.getString();
  			}

		}else if (httpCode <= 0){
			// エラー処理があれば入れる
			canvas.drawString("ERR: failed request to HTTP: "+url, 0, 0);
			M5.shutdown(60);
		}
		http.end();
	}
	virtual void read_json(){}
	String weatherCodeToPngFile(){
		DynamicJsonDocument doc(4096*4);
		String buf;
		File file;
    	file = SD.open("/weatherCode.json",FILE_READ);
		while(file.available()){
    		buf=buf+file.readString();
    	}
		file.close();
		deserializeJson(doc, buf);
		return doc[0][weather_code][0];
	}
#define GET(NAME) decltype(NAME) get_##NAME(){return NAME;}
	GET(date)
	GET(forecast);
	GET(weather_code);
	GET(pop_dates);
	GET(pops);
	GET(temp_date);
	GET(temp_min);
	GET(temp_max);
	GET(payload);
#undef GET
	void draw(const int draw_height, String prefacture_name){
		canvas.setTextFont(2);
    	canvas.setTextSize(2);

		//block 1, prefacture name
		canvas.drawString(prefacture_name, w_interval*2, draw_height+h_interval*2);

		//block 2, temperature
		canvas.drawString(this->get_temp_max()+" `C", w_interval*2+block_width+100, draw_height+h_interval*3+w_interval/2);
		canvas.drawString(this->get_temp_min()+" `C", w_interval*2+block_width+100, draw_height+h_interval*3+w_interval/2+50);

		//block 3, chance of rain
		int sum=0;
		for(int i=0;i<this->get_pops().size();++i){
			auto date=this->get_pop_dates().at(i);
			date=date.substring(11,13);
			if(date=="00"){
				date="00-06:";
			}else if(date=="06"){
				date="06-12:";
			}else if(date=="12"){
				date="12-18:";
			}else if(date=="18"){
				date="18-24:";
			}
			canvas.drawString(date, w_interval*2+block_width*2, draw_height+h_interval*(3+i));
			canvas.drawString(this->get_pops().at(i)+"%", w_interval*2+block_width*2+100, draw_height+h_interval*(3+i));
			sum+=this->get_pops().at(i).toInt();
		}

		//block 4, pops avg
		int pops_avg=sum/this->get_pops().size();
		canvas.drawString(String(pops_avg)+" %", w_interval*2+block_width*3+50, draw_height+block_height+h_interval);

		//block 1, wether icon
		String png_name="/weathercode/png/"+this->weatherCodeToPngFile();
		canvas.drawPngFile(SD,png_name.c_str(),w_interval+(block_width-512*0.2)/2,draw_height+h_interval*3,120,120,0,0,0.2);

		//block 2, temperature icon
		canvas.drawPngFile(SD,"/images/thermometer.png",w_interval*2+block_width,draw_height+h_interval*3,120,120,0,0,0.2);

		//block 4, umbrella icon
		String unbrella_name;
		if(pops_avg<30){
			unbrella_name="umbrella_stand.png";
		}else if(pops_avg<60){
			unbrella_name="umbrella_close.png";
		}else{
			unbrella_name="umbrella_open.png";
		}
		canvas.drawPngFile(SD,("/images/"+unbrella_name).c_str(),w_interval+block_width*3+(block_width-512*0.2)/2,draw_height+h_interval*3,120,120,0,0,0.2);

		//block 1, forcast msg
		canvas.loadFont("/font.ttf", SD);
		canvas.createRender(30, 256);
    	canvas.setTextSize(30);
		canvas.setTextWrap(true, true);
		canvas.setTextArea(w_interval*2, draw_height+block_height+w_interval*2/3, block_width*2-w_interval,block_height);
		forecast.replace("　"," ");
		canvas.println(this->get_forecast());
		canvas.unloadFont();
	}
};

class Tokyo: public PrefectureBase{
public:
	Tokyo(const String _url):PrefectureBase(_url){read_json();}
	void read_json(){
		DynamicJsonDocument doc(4096*4);
		deserializeJson(doc, payload);
		int num=0;
		for(int i=0;i<3;++i){
			num=i;
			String _date = doc[0]["timeSeries"][0]["timeDefines"][i];
			auto day=_date.substring(8,10);
			if(day==getRTCDay())break;
		}
		String _forecast = doc[0]["timeSeries"][0]["areas"][0]["weathers"][num];
		forecast=_forecast;
		String _weather_code = doc[0]["timeSeries"][0]["areas"][0]["weatherCodes"][num];
		weather_code=_weather_code;
		for(int i=0;i<5;++i){
			String pop_date = doc[0]["timeSeries"][1]["timeDefines"][i];
			auto date_day=getRTCDay();
			auto pop_date_day=pop_date.substring(8,10);
			if(date_day==pop_date_day){
				pop_dates.push_back(pop_date);
				String pop = doc[0]["timeSeries"][1]["areas"][0]["pops"][i];
				pops.push_back(pop);
			}
		}

		String _temp_date = doc[0]["timeSeries"][2]["timeDefines"][0];
		temp_date=_temp_date;
		String _temp_min = doc[0]["timeSeries"][2]["areas"][0]["temps"][0];
		temp_min=_temp_min;
		String _temp_max = doc[0]["timeSeries"][2]["areas"][0]["temps"][1];
		temp_max=_temp_max;
		if(temp_min==temp_max)temp_min="-";
	}

};
class Saitama: public PrefectureBase{
public:
	Saitama(const String _url):PrefectureBase(_url){read_json();}
	void read_json(){
		DynamicJsonDocument doc(4096*4);
		deserializeJson(doc, payload);
		int num=0;
		for(int i=0;i<3;++i){
			num=i;
			String _date = doc[0]["timeSeries"][0]["timeDefines"][i];
			auto day=_date.substring(8,10);
			if(day==getRTCDay())break;
		}
		String _forecast = doc[0]["timeSeries"][0]["areas"][1]["weathers"][num];
		forecast=_forecast;
		String _weather_code = doc[0]["timeSeries"][0]["areas"][1]["weatherCodes"][num];
		weather_code=_weather_code;
		for(int i=0;i<5;++i){
			String pop_date = doc[0]["timeSeries"][1]["timeDefines"][i];
			auto date_day=getRTCDay();
			auto pop_date_day=pop_date.substring(8,10);
			if(date_day==pop_date_day){
				pop_dates.push_back(pop_date);
				String pop = doc[0]["timeSeries"][1]["areas"][1]["pops"][i];
				pops.push_back(pop);
			}
		}

		String _temp_date = doc[0]["timeSeries"][2]["timeDefines"][0];
		temp_date=_temp_date;
		String _temp_min = doc[0]["timeSeries"][2]["areas"][1]["temps"][0];
		temp_min=_temp_min;
		String _temp_max = doc[0]["timeSeries"][2]["areas"][1]["temps"][1];
		temp_max=_temp_max;
		if(temp_min==temp_max)temp_min="-";
	}

};



void setup()
{
    M5.begin();
    M5.EPD.SetRotation(0);
    M5.EPD.Clear(true);
    M5.RTC.begin();
    canvas.createCanvas(display_width, display_height);
	canvas.setTextFont(2);
    canvas.setTextSize(2);

	float battery=getBatteryRaw();
    canvas.drawString(getBatteryPercentage(), display_width-h_interval*3,0);
	canvas.drawRect( display_width-h_interval*1-w_interval,w_interval/2,25,w_interval,15);
	canvas.fillRect( display_width-h_interval*1-w_interval+25,w_interval*2/3,5,w_interval*2/3,15);
	canvas.fillRect( display_width-h_interval*1-w_interval+25*(1.0-battery),w_interval/2,25*battery,w_interval,15);



    canvas.drawString("Weather", w_interval*2, h_interval+w_interval/3);
    canvas.drawString("Temperature", w_interval*2+block_width, h_interval+w_interval/3);
    canvas.drawString("Rain", w_interval*2+block_width*2, h_interval+w_interval/3);
    canvas.drawString("Umbrella", w_interval*2+block_width*3, h_interval+w_interval/3);
	

	WiFi.begin("WIFI", "PASSWORD");
	unsigned long startTime = millis();

	M5.BatteryADCBegin();
	
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
		if(millis() - startTime > 10000){     // タイムアウト(10秒)
    	// タイムアウト処理があれば入れる
    		canvas.drawString("ERR: Wifi timed out.", 0, 0);
			break;
  		}
    }
	if(WiFi.status() == WL_CONNECTED){
		//time
		setRTCTimeFromNtp();
		String timeStr=getRTCTimeWithFormat();
   		canvas.drawString("update date: "+timeStr, 0, 0);

		String url="https://www.jma.go.jp/bosai/forecast/data/forecast/130000.json";
		Tokyo t(url);
		t.draw(0,"Tokyo");
		url="https://www.jma.go.jp/bosai/forecast/data/forecast/110000.json";
		Saitama s(url);
		s.draw(block_height,"Saitama");




	}

	// line layout
	canvas.drawRect(w_interval, h_interval, display_width-w_interval*2, display_height-w_interval-h_interval, 15);//outside rectangle
    canvas.drawLine(w_interval, h_interval*2, display_width-w_interval, h_interval*2, 15);//horizonal line 1
	canvas.drawLine(w_interval, block_height+h_interval*2, display_width-w_interval, block_height+h_interval*2, 15);//horizonal line 2
	canvas.drawLine(w_interval, block_height+w_interval/2, w_interval+block_width*2, block_height+w_interval/2, 15);//horizonal line 1 buttom
	canvas.drawLine(w_interval, block_height*2+w_interval/2, w_interval+block_width*2, block_height*2+w_interval/2, 15);//horizonal line 2 buttom
	canvas.drawLine(w_interval+block_width, h_interval, w_interval+block_width, block_height+w_interval/2, 15);//vertical line 1 top
	canvas.drawLine(w_interval+block_width, block_height+h_interval*2, w_interval+block_width, block_height*2+w_interval/2, 15);//vertical line 1 buttom
	canvas.drawLine(w_interval+block_width*2, h_interval, w_interval+block_width*2, display_height-w_interval, 15);//vertical line 2
	canvas.drawLine(w_interval+block_width*3, h_interval, w_interval+block_width*3, display_height-w_interval, 15);//vertical line 3
	
	canvas.pushCanvas(0,0,UPDATE_MODE_DU4);
}

void loop()
{
	delay(60*1000);//60 sec
	//deepsleep
	WiFi.disconnect(true);
	rtc_time_t RTCtime;
	M5.RTC.getTime(&RTCtime);

	RTCtime.hour=2;
	RTCtime.min=0;
	RTCtime.sec=0;
	M5.shutdown(RTCtime);
}