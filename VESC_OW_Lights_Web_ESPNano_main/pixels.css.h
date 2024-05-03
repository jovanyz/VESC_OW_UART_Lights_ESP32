const char* pixels_css = R"=====(
/* Set padding and margins to 0 then change later
Helps with cross browser support */
* {
 padding:0;
 margin:0;
}


li, dd { margin-left:5%; }

fieldset { padding: .5em; }

br.all {
  clear: both;
}

br.clearall {
  clear: both;
}


/* remove margin (added above) on nested ul */
ul ul { margin: 0;}
ul ul ul { margin: 0;}


/* Set selected button color */
.mode-selected {
    background: #AAAAAA;
}

.mode-select-div {
    display: inline-block;
}

.mode-select-btn {
    border: none;
    padding: 15px;
    text-align: center;
    font-size: 14pt;
    margin: 10px;
    cursor: pointer;
    width:  150px;
    height: 75px;
    border-radius: 25%
}

#modes-list {
    list-style-type: none;
    margin: 30 px;
    padding: 30 px;
    overflow: hidden;
}

.li-mode-select {
    float: left;
    padding: 0;
    margin: 0;
}


#divapplybuttons {
    background-color: #444444;
}

#applybutton {
    background-color: #ccff55;
    border: none;
    padding: 15px;
    text-align: center;
    font-size: 16pt;
    margin: 10px;
    cursor: pointer;
    width:  150px;
    height: 90px;
}

#onoffbutton {
    border: none;
    padding: 15px;
    text-align: center;
    font-size: 16pt;
    margin: 10px;
    cursor: pointer;
    width:  120px;
    height: 90px;
    border-radius: 45%;
}

#otabutton {
    border: none;
    padding: 15px;
    text-align: center;
    font-size: 16pt;
    margin: 10px;
    cursor: pointer;
    width:  120px;
    height: 90px;
    border-radius: 45%;
}

#blebutton {
    border: none;
    padding: 15px;
    text-align: center;
    font-size: 16pt;
    margin: 10px;
    cursor: pointer;
    width:  120px;
    height: 90px;
    border-radius: 45%;
}

.brightness-slider {
  -webkit-appearance: none;  /* Override default CSS styles */
  appearance: none;
  width: 75%; /* 3/4-width */
  height: 20px; /* Specified height */
  border-radius: 5px;
  background: #d3d3d3; /* Grey background */
  outline: none; /* Remove outline */
  opacity: 0.7; /* Set transparency (for mouse-over effects on hover) */
  -webkit-transition: .2s; /* 0.2 seconds transition on hover */
  transition: opacity .2s;
}

.brightness-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 40px;
  height:40px;
  border-radius: 50%; 
  background: #aa0449;
  cursor: pointer;
}

.brightness-slider::-moz-range-thumb {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: #aa0449;
  cursor: pointer;
}

.brightness-slider:hover {
  opacity: 1; /* Fully shown on mouse-over */
}

.onoff-selected {
    background-color: #aaaaaa;
}

.ota-selected {
    background-color: #ff4444;
}

.ble-selected {
    background-color: #4444ff;
}

.onoff-not-selected {
    background-color: #ffffff;
}

.ota-not-selected {
    background-color: #ffaaaa;
}

.ble-not-selected {
    background-color: #aaaaff;
}

input[type="color"] {
	-webkit-appearance: none;
  background-image: linear-gradient(to right, Red , Orange, Yellow, Green, Blue, Indigo, Violet);
  berder-radius: 5px;
	width: 100px;
	height: 100px;
}

.input {
    padding-bottom: 30px;
}

p {
  -webkit-appearance: none;
  margin-bottom: 20px;
}

)=====";
