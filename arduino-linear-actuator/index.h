// HTML web page
// Be wary of percent signs since the processor function will try to replace them
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
    <title>Greenhouse Window Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      h1 { 
        color: green;
      }

      h2 {
        margin-top: 5px;
        margin-bottom: 5px;
      }

      body { 
        color: white;
        max-width: 800px;
        background-color: #333c4d;
        font-family: Arial; 
        text-align: center; 
        margin:0px auto; 
        padding-top: 30px;
      }

      span#state_string {
        font-weight: bold;
      }

      span#temperature {
        font-weight: bold;
      }

      #temperature_controls input {
        width: 60px;
      }

      section {
        background-color: #f7fafc;
        color: #000000;
        padding: 5px;
        margin: 5px;
        border-radius: 5px;
      }

      #motor_controls button {
        background-color: #333c4d;
        color: white;
        height: 80px;
        width: 180px;
        padding: 20px;
        margin: 5px;
        font-size: 24px;
        font-weight: bold;
        text-align: center;
        outline: none;
        border-width: 2px;
        border-style: solid;
        border-color: white;
        border-radius: 5px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
        -webkit-appearance: none;
      }

      button.running {
        background-color: #1fe036 !important;
      }

      button.stopped {
        background-color: lightgray !important;
      }

      button:hover {
        border-color: black;
      }

      button#led {
        color: #000000;
        background-color: #20aeff;
      }

      button#led:active {
        background-color: #24262b;
      }

      button#stop {
        color: #ffffff;
        background-color: #ff0522;
      }

      .note {
        font-weight: normal;
        font-size: 12px;
      }

      .switch {
        position: relative;
        display: inline-block;
        width: 60px;
        height: 34px;
      }

      .switch input { 
        opacity: 0;
        width: 0;
        height: 0;
      }

      .slider {
        position: absolute;
        cursor: pointer;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background-color: #ccc;
        -webkit-transition: .4s;
        transition: .4s;
      }

      .slider:before {
        position: absolute;
        content: "";
        height: 26px;
        width: 26px;
        left: 4px;
        bottom: 4px;
        background-color: white;
        -webkit-transition: .4s;
        transition: .4s;
      }

      input:checked + .slider {
        background-color: #2196F3;
      }

      input:focus + .slider {
        box-shadow: 0 0 1px #2196F3;
      }

      input:checked + .slider:before {
        -webkit-transform: translateX(26px);
        -ms-transform: translateX(26px);
        transform: translateX(26px);
      }

      /* Rounded sliders */
      .slider.round {
        border-radius: 34px;
      }

      .slider.round:before {
        border-radius: 50%%;
      }

      /* The snackbar - position it at the bottom and in the middle of the screen */
      /* https://www.w3schools.com/howto/howto_js_snackbar.asp */
      #snackbar {
        visibility: hidden; /* Hidden by default. Visible on click */
        max-width: inherit; /* Set a default minimum width */
        background-color: red; /* Black background color */
        color: #fff; /* White text color */
        text-align: center; /* Centered text */
        padding-top: 16px;
        padding-bottom: 16px;
        position: fixed; /* Sit on top of the screen */
        z-index: 1; /* Add a z-index if needed */
        top: 30px; /* 30px from the bottom */
      }

      /* Show the snackbar when clicking on a button (class added with JavaScript) */
      #snackbar.show {
        visibility: visible; /* Show the snackbar */
      }
    </style>
  </head>

  <body>
    <h1>Greenhouse Window Controller</h1>
    <div id="top_controls">
      <p>State: <span id="state_string">%STATE%</span></p>
      </br>
      <span>Automatic Thermostat Mode:</span>
      <label class="switch">
        <input id="automatic" type="checkbox" onchange="toggleCheckbox(this);" %AUTOMATIC_CHECKED%>
        <span class="slider round"></span>
      </label>
    </div>
    <section id="temperature_controls">
      <h2>Temperature Controls</h2>
        <p>
          Current Temperature: <span id="temperature">%TEMPERATURE%</span> &deg;F
        </p>

        <form id="temp_form">
          <p>
            <label for="turn_off">Open Above: </label>
            <input id="turn_off" type="number" value="%TURN_OFF%" /> &deg;F
          </p>
          <p>
            <label for="turn_on">Close Below: </label>
            <input id="turn_on" type="number" value="%TURN_ON%" /> &deg;F
          </p>
          <button id="update_setpoints" type="submit">Update Setpoints</button>
      </form>
    </section>

    <section id="motor_controls">
      <h2>Motor Controls</h2>    
      <button id="retract" onmousedown="pressButton(this);">
        RETRACT
        <br/>
        <span class="note" id="lower_limit">%LOWER_LIMIT%</span>
      </button>

      <button id="extend" onmousedown="pressButton(this);">
        EXTEND
        <br/>
        <span class="note" id="upper_limit">%UPPER_LIMIT%</span>
      </button>
      <br/>
      <button id="stop" onmousedown="pressButton(this);">
        STOP
      </button>
    </section>

    <section id="misc_controls">
      <button id="led" onmousedown="ledButton('ON');" ontouchstart="ledButton('ON');" onmouseup="ledButton('OFF');" ontouchend="ledButton('OFF');">
        LED
        <br/>
        <span class="note">(Momentary Off)</span>
      </button>
    </section>

    <footer>
      <p>
        %VERSION% &emsp;
        <a href="/update">Update</a> &emsp;
        &hearts; Ryan Reedy
      </p>
    </footer>

    <div id="snackbar">DISCONNECTED! Don't press buttons too fast.</div>

  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');
      
      source.addEventListener('open', function(e) {
        console.log("Events Connected");
        document.getElementById("snackbar").className = "";
      }, false);
      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
          document.getElementById("snackbar").innerHTML = "DISCONNECTED! Don't press buttons too fast.";
          document.getElementById("snackbar").className = "show";
        }
      }, false);
      
      source.addEventListener('message', function(e) {
        console.log("message", e.data);
      }, false);

      source.addEventListener('degC', function(e) {
        console.log("upper_limit", e.data);
        document.getElementById("degC").innerHTML = e.data;
      }, false);

      source.addEventListener('state', function(e) {
        var state = String(e.data);
        console.log("state", state);
        document.getElementById("state_string").innerHTML = state;
        switch (state){
          case 'STOPPED':
            document.getElementById("extend").className = "";
            document.getElementById("retract").className = "";
            break;
          case 'WINDOW_OPEN':
            document.getElementById("extend").className = "stopped";
            document.getElementById("retract").className = "";
            break;
          case 'WINDOW_CLOSED':
            document.getElementById("extend").className = "";
            document.getElementById("retract").className = "stopped";
            break;
          case 'EXTENDING':
            document.getElementById("extend").className = "running";
            document.getElementById("retract").className = "";
            break;
          case 'RETRACTING':
            document.getElementById("extend").className = "";
            document.getElementById("retract").className = "running";
            break;
          case 'TIMEOUT':
            document.getElementById("extend").className = "";
            document.getElementById("retract").className = "";
            document.getElementById("automatic").checked = false;
            break;
          case 'ERROR':
            document.getElementById("extend").className = "stopped";
            document.getElementById("retract").className = "stopped";
            break;
          case 'DEFAULT':
          default:
            console.log("Switch case fell through");
        }
      }, false);               
    }

    function toggleCheckbox(element) {
      var xhr = new XMLHttpRequest();
      if(element.checked){ xhr.open("GET", "/checkbox?output="+element.id+"&state=1", true); }
      else { xhr.open("GET", "/checkbox?output="+element.id+"&state=0", true); }
      xhr.send();
    }

    function ledButton(x) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/" + x, true);
      xhr.send();
    }

    function pressButton(element){
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/command?type=" + element.id, true);
      if (element.id == "stop"){
        document.getElementById("automatic").checked = false;
      }
      xhr.send();
    }

    function setpointsButton(e){
      var xhr = new XMLHttpRequest();
      var turn_off = document.getElementById("turn_off").value;
      var turn_on = document.getElementById("turn_on").value;

      if (parseFloat(turn_off) > parseFloat(turn_on)) {
        xhr.open("GET", "/setpoints?off=" + turn_off + "&on=" + turn_on, true);
        xhr.send();
      } else {
        var snackbar = document.getElementById("snackbar");
        snackbar.innerHTML = "ERROR! Off setpoint must be above on setpoint.";
        console.log("turn_off=", turn_off, " turn_on=", turn_on);
        snackbar.className = "show";
        // After 3 seconds, remove the show class from DIV
        setTimeout(function(){ snackbar.className = ""; }, 5000);
      }
    }

    setInterval(function ( ) {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("temperature").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/temperature", true);
      xhttp.send();
    }, 10000 ) ;

    document.getElementById("temp_form").addEventListener("submit", setpointsButton);

  </script>
  </body>
</html>)rawliteral";