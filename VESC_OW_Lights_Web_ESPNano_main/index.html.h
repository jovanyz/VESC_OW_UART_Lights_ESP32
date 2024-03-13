// store index.html as a string literal
const char* index_html = R"=====(
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="utf-8">
        <title>VESC Light Controller</title>
        <link href="pixels.css" rel="stylesheet" type="text/css" />
        <script src="jquery.min.js" type="text/javascript"></script>
        <script src="jquery-ui.min.js"></script>
        <script src="pixels.js" type="text/javascript"></script>
        </head>

    <body>
        <div id="divapplybuttons"><button type="button" id="applybutton" onclick="apply()">Apply</button><button type="button" id="onoffbutton" class="onoff-not-selected" onclick="onoff_toggle()">Turn Off</button></div>
        <div style="text-align:center" id="modes">
          <p></p>
          <h2>Modes</h2>
          <p></p>
          <ul id="modes-list">
          </ul>
        </div>
        <p></p>
        <div id="options">
            <div style="text-align:center" id="brightnessoption">
              <h2>Brightness</h2>
              <input type="range" min="0" max="100" value="0" class="brightness-slider" id="brightness" onchange="show_brightness()"/>
              <p>Brightness: <span id="brightness-val"> 90 </span>%</p>
        </div >
        <div style="text-align:center" id = colorpicker">
          <h2> Color Picker </h2>
          <input type="color" id="colorpicker" value="#ffffff"/>
        </div>
    </body>
</html>

)=====";
