// One countdown for every screen's logout view: a key pressed on any of them stops all of them.
.pragma library

var callbacks = [];

function addCancelAutoTriggerCallback(callback) {
    callbacks.push(callback);
}

function cancelAutoTrigger() {
    callbacks.forEach(function (callback) { if (callback) callback(); });
}
