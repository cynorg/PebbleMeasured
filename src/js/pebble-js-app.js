var locationOptions = { "timeout": 15000, "maximumAge": 1000, "enableHighAccuracy": true }; // 15 second timeout, allow 1 second cached
//var cachedLocationOptions = { "timeout": 0, "maximumAge": 600000, "enableHighAccuracy": false }; // allow 10 min cached
var locationWatcher;
var lastCoordinates;
var weatherFormat;

Pebble.addEventListener("ready", function (e) {
//    console.log("Connect! " + e.ready);
//    locationWatcher = window.navigator.geolocation.watchPosition(weatherLocationSuccess, locationError, locationOptions);
//    navigator.geolocation.clearWatch(locationWatcher);
    getWatchVersion();
});

Pebble.addEventListener("showConfiguration", function () {
    //console.log("Configuration window launching...");
    var baseURL, pebtok, nocache;
    baseURL = 'http://www.cyn.org/pebble/measured/';
    pebtok  = '&pat=' + Pebble.getAccountToken();
    nocache = '&_=' + new Date().getTime();
    if (window.localStorage.getItem("version_config") !== null) {
        Pebble.openURL(baseURL + window.localStorage.version_config + ".html?" + pebtok + nocache);
        console.log(baseURL + window.localStorage.version_config + ".html?" + pebtok + nocache);
    } else { // in case we never received the message / new install
        Pebble.openURL(baseURL + "0.1.html?" + pebtok + nocache);
        console.log(baseURL + "0.1.html?" + pebtok + nocache);
    }
});

function getWatchVersion() {
    Pebble.sendAppMessage({ message_type: 104 },
        function (e) {
            console.log("Sent watch version request with transactionId=" + e.data.transactionId);
        },
        function (e) {
            console.log("Unable to deliver watch version request message with transactionId=" + e.data.transactionId + " Error is: " + e.data.error.message);
        }
        );
}

function saveWatchVersion(e) {
    console.log("Watch Version: " + e.payload.send_watch_version);
    console.log("Config Version: " + e.payload.send_config_version);
    window.localStorage.version_watch = e.payload.send_watch_version;
    window.localStorage.version_config = e.payload.send_config_version;
}

function sendTimezoneToWatch() {
    var offsetQuarterHours = Math.floor(new Date().getTimezoneOffset() / 15);
    // UTC offset in quarter hours ... 48 (UTC-12) through -56 (UTC+14) are the valid ranges
    Pebble.sendAppMessage({ message_type: 103, timezone_offset: offsetQuarterHours },
        function (e) {
            console.log("Sent TZ message (" + offsetQuarterHours + ") with transactionId=" + e.data.transactionId);
        },
        function (e) {
            console.log("Unable to deliver TZ message with transactionId=" + e.data.transactionId + " Error is: " + e.data.error.message);
        }
        );
}

Pebble.addEventListener("appmessage", function (e) {
    //console.log("Received message: type " + e.payload.message_type)
    switch (e.payload.message_type) {
    case 103:
        sendTimezoneToWatch();
        break;
    case 104:
        saveWatchVersion(e);
        sendTimezoneToWatch(); // a little bonus, since we know the watch is listening
        break;
    }
});

Pebble.addEventListener("webviewclosed", function (e) {
    //console.log("Configuration closed");
    //console.log("Response = " + e.response.length + "   " + e.response);
    if (e.response !== undefined && e.response !== '' && e.response !== 'CANCELLED') { // user clicked Save/Submit, not Cancel/Done
        var options;
        options = JSON.parse(e.response);
        window.localStorage.measured_options = JSON.stringify(options);
        Pebble.sendAppMessage(options,
            function (e) {
                console.log("Successfully delivered message with transactionId=" + e.data.transactionId);
            },
            function (e) {
                console.log("Unable to deliver message with transactionId=" + e.data.transactionId + " Error is: " + e.data.error.message);
            }
            );
    } else if (e.response === 'CANCELLED') {
        console.log("Android misbehaving on save due to embedded space in e.response... ignoring");
    }
});

