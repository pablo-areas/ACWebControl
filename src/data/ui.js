var state = {}

function updateStatus() {
  $.ajax({
    type: 'GET',
    url: "state",
    dataType: "json",
    data: [{
      name: "ambtemp",
      value: "20"
    }, ],
    success: function(data) {
      if (!data) {
        return;
      }
      state = data;

      if (state["power"] === true) {
        $("#power").text(" ON");
        $("#power-btn").addClass("btn-info");
        $("#power-btn").removeClass("btn-default");
      } else {
        $("#power").text(" OFF");
        $("#power-btn").addClass("btn-default");
        $("#power-btn").removeClass("btn-info");
      }
      
      $("#ambient_temp").text(state["ambtemp"] + " C");
      $("#target_temp").text(state["temp"] + " C");
      setModeColor(state["mode"]);
      setFanColor(state["fan"]);
      setTimerOffColor(state["timeroff"]);
    },
    error: function() {
      console.log('error getting state');
    },
    timeout: 1000
  });
}

updateStatus();

function postData(t) {
  var e = new XMLHttpRequest;
  e.timeout = 2000;
  e.open("PUT", "state", !0);
  e.setRequestHeader("Content-Type", "application/json");
  console.log(JSON.stringify(t)), e.send(JSON.stringify(t));
}

function mode_onclick(mode) {
  state["mode"] = mode;
  setModeColor(mode);
  postData(state);
}

function setModeColor(mode) {
  $(".mode-btn").addClass("btn-default");
  $(".mode-btn").removeClass("btn-info");

  if (mode === 0) {
    $("#mode_auto").removeClass("btn-default");
    $("#mode_auto").addClass("btn-info");
    setFanColor(0);
    state["fan"] = 0;
  } else if (mode === 1) {
    $("#mode_cooling").removeClass("btn-default");
    $("#mode_cooling").addClass("btn-info");
  } else if (mode === 2) {
    $("#mode_dehum").removeClass("btn-default");
    $("#mode_dehum").addClass("btn-info");
  } else if (mode === 3) {
    $("#mode_heating").removeClass("btn-default");
    $("#mode_heating").addClass("btn-info");
  } else if (mode === 4) {
    $("#mode_fan").removeClass("btn-default");
    $("#mode_fan").addClass("btn-info");
  }
}

function setFanColor(fan) {
  if (fan == 0) {
    $("#fan_auto").removeClass("btn-default");
    $("#fan_auto").addClass("btn-info");
  } else {
    $("#fan_auto").removeClass("btn-info");
    $("#fan_auto").addClass("btn-default");
  }
  for (var i = 1; i <= 3; ++i) {
    if (i <= fan) {
      $("#fan_lvl_" + i).attr("src", "level_" + i + "_on.svg");
    } else {
      $("#fan_lvl_" + i).attr("src", "level_" + i + "_off.svg");
    }
  }
}

function fan_onclick(fan) {
  if (state["mode"] !== 0) {
    state["fan"] = fan;
    setFanColor(fan);
    postData(state);
  }
}

function setTimerOffColor(timeroff){
  for (var i=1; i <= 3; i++){
    $("#toff" + i).removeClass("btn-info");
    $("#toff" + i).addClass("btn-default");
  }

  switch (timeroff) {
    case 1:
        $("#toff1").removeClass("btn-default");
        $("#toff1").addClass("btn-info");
        break;
    case 2:
        $("#toff2").removeClass("btn-default");
        $("#toff2").addClass("btn-info");
        break;
    case 3:
        $("#toff3").removeClass("btn-default");
        $("#toff3").addClass("btn-info");
        break;
    default:
      break;
  }
}

function timeroff_onclick(timeroff){
  if (state["power"] === true) {
    if (state["timeroff"] == timeroff){
      timeroff = 0;
    }

    state["timeroff"] = timeroff;      

    setTimerOffColor(timeroff)
    postData(state);
  }
}

function power_onclick(power) {
  if (state["power"]) {
    state["power"] = false;
    $("#power").text(" OFF");
    $("#power-btn").removeClass("btn-info");
    $("#power-btn").addClass("btn-default");
    $("#led-btn").addClass("hide");
  } else {
    state["power"] = true;
    $("#power").text(" ON");
    $("#power-btn").addClass("btn-info");
    $("#power-btn").removeClass("btn-default");
    state["led"] = true;
    $("#led-btn").removeClass("hide");
  }

  state["timeroff"] = 0;
  setTimerOffColor(0);

  postData(state);
}

function temp_onclick(temp) {
  state["temp"] += temp;
  if (state["temp"] < 17) {
    state["temp"] = 17;
  }
  if (state["temp"] > 30) {
    state["temp"] = 30;
  }
  $("#target_temp").text(state["temp"] + " C");
  postData(state);
}

function led_onclick(led) {
  if (state["led"]) {
    state["led"] = false;
  } else {
    state["led"] = true;
  }
  postData(state);
}