// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getValues(){
    var jsonString = createPackage('update', 0, 0);
    websocket.send(jsonString);
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function updateSliderPWM(element) {
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById(element.id + "-value").innerHTML = sliderValue;
    console.log(sliderValue);
    // websocket.send(sliderNumber+"s"+sliderValue.toString());
}

function createPackage(action, setTemperature, setTime) {
    var jsonString = JSON.stringify({
        'action': action,
        'setTemperature': setTemperature,
        'setTime': setTime
    })

    return jsonString;
}

function submitForm(){

    var setTemperature = document.querySelector('#slider-temp').value
    var setTime = document.querySelector('#slider-time').value

    // console.table({
    //     temp: setTemperature,
    //     time: setTime
    // })

    var jsonString = createPackage('heat', setTemperature, setTime);

    console.log(jsonString);

    websocket.send(jsonString);
}

function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    // for (var i = 0; i < keys.length; i++){
    //     var key = keys[i];
    //     document.getElementById(key).innerHTML = myObj[key];
    //     document.getElementById("slider"+ (i+1).toString()).value = myObj[key];
    // }
    document.getElementById('machine-state').innerText = myObj['state']
    document.getElementById('slider-temp').value = myObj['setTemp']
    document.getElementById('slider-time').value = myObj['setTime']
    document.getElementById("slider-temp-value").innerHTML = myObj['setTemp'];
    document.getElementById("slider-time-value").innerHTML = myObj['setTime'];
}

/**
 * Temperature Chart
 */

var chartT = new Highcharts.Chart({
    chart:{ renderTo : 'chart-temperature' },
    title: { text: 'Inside Temperature Chart' },
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
        title: { text: 'Temperature (Celsius)' }
        //title: { text: 'Temperature (Fahrenheit)' }
    },
    credits: { enabled: false }
});

function updateTemperatureChart() {
    var x = (new Date()).getTime(),
        y = parseFloat(Math.random().toFixed(2));
    //console.log(this.responseText);
    if(chartT.series[0].data.length > 40) {
        chartT.series[0].addPoint([x, y], true, true, true);
    } else {
        chartT.series[0].addPoint([x, y], true, false, true);
    }
}

setInterval(updateTemperatureChart, 3000) ;


