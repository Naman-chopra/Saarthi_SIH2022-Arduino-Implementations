#include <Ultrasonic.h>
#include <string.h>
#include <TinyGPS++.h> // library for GPS module
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
const char* ssid = "m51";
const char* password = "m51854269";
const char * myWriteAPIKey = "G9GNVJGKQKQG3OSH";
const char* host = "script.google.com";
const int httpsPort = 443;
String readString;


const char* fingerprint = "46 B2 C3 44 9C 59 09 8B 01 B6 F8 BD 4C FB 00 74 91 2F EF F6";
String GAS_ID = "AKfycbxPlm4J24pQglKF2KU8kHSfo8sDFuu9czrduelziNMtrwbS4AjYDDBNdBp7NVEIbMKw";  // Replace by your GAS service id
unsigned long myChannelNumber = 1830210;

float d;
double latitude,longitude;
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str, lat_lon="", global_ret="";
int pm;
AsyncWebServer server(80);
Ultrasonic ultrasonic (D6, D7);///D6 for trigger
TinyGPSPlus gps;  /// D1 for RX
SoftwareSerial ss(4, 5);
WiFiClient  client;
WiFiClientSecure client_GS;

void sendData(String x, String y, float z);

////////////////////////////////////////////////////////////////////////////////////////////////////////

void Merge(String s1, String s2){
  String s3="";
  s3 += s1+","+s2;
  Serial.println("\n");
  Serial.println(s3);
  if(s2==lat_lon){
    global_ret=s3;
  }
}
//////////////////////////////////////////////////
void getDistance() {
  // Read Distance
d= ultrasonic.read();
   if((d>31||d<28)&&(d<50)) {
     ThingSpeak.setField(3, d);
     ThingSpeak.setField(1, lat_str);
     ThingSpeak.setField(2, lng_str);
     ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
     String disstance=String(d);
    Merge(disstance,lat_lon);
//    global_ret=disstance;
    sendData(lat_str,lng_str,d);
  }
}

/////////////////////////////////////////////////
void sendData(String  x, String y, float z)
{
  client_GS.setInsecure();
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client_GS.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

//  if (client_GS.verify(fingerprint, host)) {
//  Serial.println("certificate matches");
//  } else {
//  Serial.println("certificate doesn't match");
//  }
  String string_x     =  x;
  String string_y     =  y;
  String string_z     =  String(z, DEC);
  String url = "/macros/s/" + GAS_ID + "/exec?Latitude=" + string_x + "&Longitude=" + string_y + "&Depth="+string_z;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client_GS.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client_GS.connected()) {
  String line = client_GS.readStringUntil('\n');
  if (line == "\r") {
    Serial.println("headers received");
    break;
  }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const char index_html[] PROGMEM= R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src="https://code.highcharts.com/highcharts.js"></script>
  <style>
    body {
      min-width: 310px;
      max-width: 800px;
      height: 400px;
      margin: 0 auto;
    }
    h2 {
      font-family: Arial;
      font-size: 2.5rem;
      text-align: center;
    }
  </style>
</head>
<body>
  <h2>ESP8266 Distance Plot</h2>
  <div id="chart-distance" class="container"></div>
</body>
<script>
let csvDataArr = [];
var chartT = new Highcharts.Chart({
  chart:{ renderTo : 'chart-distance' },
  title: { text: 'HC-SR04 Distance' },
  series: [{
    showInLegend: false,
    data: []
  }],
  plotOptions: {
    line: { animation: false,
      dataLabels: { enabled: true }
    },
    series: { color: '#059e8a' }
  },
  xAxis: { type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  },
  yAxis: {
    title: { text: 'Distance (CM)' }
  },
  credits: { enabled: false }
});
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  // console.log("hello");
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
     var x = (new Date()).getTime(),
          y = (this.responseText);
          console.log(y);
          var dis = y.split(',')[0];
          var plot = parseFloat(y.split(',')[0]);
         if (dis>20){
           csvDataArr.push([y]);
         }
      if(chartT.series[0].data.length > 40) {
        chartT.series[0].addPoint([x, plot], true, true, true);
      } else {
        chartT.series[0].addPoint([x, plot], true, false, true);
      }
    }
  };
  xhttp.open("GET", "/distance", true);
  xhttp.send();
}, 1500 ) ;
setInterval (function(){

  let csvContent = "data:text/csv;charset=utf-8," 
    + csvDataArr.map(e => e.join(",")).join("\n");

var encodedUri = encodeURI(csvContent);
var link = document.createElement("a");
link.setAttribute("href", encodedUri);
link.setAttribute("download", "my_data.csv");
document.body.appendChild(link); // Required for FF

link.click();
}, 30000);

console.log('Output array', csvDataArr);

</script>
</html>
)rawliteral";

void setup () {
  // Serial port for debugging purposes
  Serial.begin (115200);
  ss.begin(9600);

  // Initialize SPIFFS
  if (! SPIFFS.begin ()) {
    Serial.println ("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());
  ThingSpeak.begin(client);

  // Route for web page
  server.on ("/", HTTP_GET, [] (AsyncWebServerRequest * request) {
        request-> send_P (200, "text / html", index_html);
  server.on ("/distance", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request-> send_P (200, "text / plain", global_ret.c_str());
  });

  });
  // start server
  server.begin ();
}

  int i=0;
void loop() {
 while (ss.available() > 0) //while data is available
    if (gps.encode(ss.read())) //read gps data
    {
      if (gps.location.isValid()) //check whether gps location is valid
      {
        latitude = gps.location.lat();
        lat_str = String(latitude , 6); // latitude location is stored in a string
        longitude = gps.location.lng();
        lng_str = String(longitude , 6); //longitude location is stored in a string
      }
    if(latitude !=0 && longitude!=0){
      lat_lon="";
      lat_lon += lat_str+","+lng_str;
      getDistance();
    }
    }

}
