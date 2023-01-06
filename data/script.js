// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
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
    sendUpdateRequest();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function updateSliderPWM(element) {
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById(element.id + "-value").innerHTML = sliderValue;
    // console.log(sliderValue);
    // websocket.send(sliderNumber+"s"+sliderValue.toString());
}

function createRequest(action, setTemperature, setTime) {
    var request = JSON.stringify({
        'action': action,
        'setTemperature': setTemperature,
        'setTime': setTime
    })
    
    return request;
}

function sendUpdateRequest(){
    var request = createRequest('update', 0, 0);
    websocket.send(request);
}

function sendRunRequest() {
    
    var setTemperature = document.querySelector('#slider-temp').value
    var setTime = document.querySelector('#slider-time').value

    var request = createRequest('run', setTemperature, setTime);
    
    console.log(request);
    
    websocket.send(request);
}

function sendStopRequest() {
    var request = createRequest('stop', 100, 0);

    console.log(request);

    websocket.send(request);
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
    document.getElementById('machine-state').innerText = myObj['state'];
    document.getElementById('machine-time-remaining').innerText = myObj['timeRemain']
    // document.getElementById('slider-temp').value = myObj['setTemp']
    // document.getElementById('slider-time').value = myObj['setTime']
    // document.getElementById("slider-temp-value").innerHTML = myObj['setTemp'];
    // document.getElementById("slider-time-value").innerHTML = myObj['setTime'];

    updateTemperatureChart(myObj['currentTemp']);
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

function updateTemperatureChart(temperature) {
    var x = (new Date()).getTime(),
        y = parseFloat(temperature);
    //console.log(this.responseText);
    if(chartT.series[0].data.length > 40) {
        chartT.series[0].addPoint([x, y], true, true, true);
    } else {
        chartT.series[0].addPoint([x, y], true, false, true);
    }
}

setInterval(sendUpdateRequest, 5000);


