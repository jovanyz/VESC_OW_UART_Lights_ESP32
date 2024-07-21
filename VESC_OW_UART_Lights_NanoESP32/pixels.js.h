const char* pixels_js = R"=====(


// Only one mode can be selected
var mode = "";
var onoff = false;
var ota = false; 
var ble = false;

$(() => {

    $('#status').html("<p>Status updated!</p>");

    $.getJSON( "modes.json", {
        tagmode: "any",
        format: "json"
    }) 
    .done(function( data ) {
        mode_name = "";
        title = "";
        description = "";
        $.each( data, function( i, mode_object ) {
            $.each (mode_object, function (key, val) {
                if (key == "mode_name") mode_name = val;
                if (key == "title") title = val;
                if (key == "description") description = val;
                if (key == "group") group = parseInt(val);
            });
            // Only show groups up to 5
            if (group < 5) {
                // Add to list
                $('#modes-list').append ("<li class=\"li-mode-select\">\n<button type=\"button\" id=\""+mode_name+"\"  title=\""+description+"\" class=\"mode-select-btn\" onclick=\"select_mode('"+mode_name+"')\">"+title+"</button>\n</li>");
                // if this is the first then set it to the mode
                if (mode == "") mode = mode_name;
            }
        });
        // Set mode 
        set_mode();
        
        
        $(".colbutton").click(function() {
            add_color($(this).attr('name'));
        });
        

    
    });
    

})

// Set all modes not selected, except one matching mode
function set_mode() {
    $.each($(".mode-select-btn"), function (i, val) {
        if (val.id != mode) {
          $(this).removeClass('mode-selected');
        }
        else {
          $(this).addClass('mode-selected');
        }
    }); 
}

function select_mode(mode_name) {
    mode = mode_name;
    set_mode();
}

function onoff_toggle() {
    if (onoff == true) {
        onoff = false;
        $("#onoffbutton").removeClass('onoff-selected');
        $("#onoffbutton").addClass('onoff-not-selected');
    }
    else {
        onoff = true;
        $("#onoffbutton").removeClass('onoff-not-selected');
        $("#onoffbutton").addClass('onoff-selected');
    }
}

function ota_toggle() {
    if (ota == true) {
        ota = false;
        $("#otabutton").removeClass('ota-selected');
        $("#otabutton").addClass('ota-not-selected');
    }
    else {
        ota = true;
        $("#otabutton").removeClass('ota-not-selected');
        $("#otabutton").addClass('ota-selected');
    }
}

function ble_toggle() {
    if (ble == true) {
        ble = false;
        $("#blebutton").removeClass('ble-selected');
        $("#blebutton").addClass('ble-not-selected');
    }
    else {
        ble = true;
        $("#blebutton").removeClass('ble-not-selected');
        $("#blebutton").addClass('ble-selected');
    }
}

function apply() {
    // Read each of the values and create the url
    // mode is in variable mode
    // get brightness
    brightness = 100 - document.getElementById("brightness").value;
    let picked_color_val = document.getElementById("colorpicker").value;
    var picked_color = picked_color_val.replace('#','');
    // onoff 
    onoff_str = 0;
    if (onoff == true) {
        onoff_str = 1;
    }
    //ota
    ota_str = 0;
    if (ota == true) {
        ota_str = 1;
    }
    //ble
    ble_str = 0;
    if (ble == true) {
        ble_str = 1;
    }
    url_string = "/set?mode="+mode+"&brightness="+brightness+"&onoff="+onoff_str+"&pickedcolor="+picked_color+"&ota="+ota_str+"&ble="+ble_str;
    $.get( url_string, function (data) {
        $("#status").html(data);
    });
    
    
}

function show_brightness() {
    // JQuery does not work well with manual slider
    // Use normal javascript to get the value
    brightness_val = document.getElementById("brightness").value;
    document.getElementById("brightness-val").innerHTML = 100-brightness_val;
    
}

)=====";
