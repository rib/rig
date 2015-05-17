var LibraryRigEmscripten = {
    $RigEmscripten__deps: ['$Browser'],
    $RigEmscripten: {

        workers: [],

    },

    rig_emscripten_worker_create: function(url) {
        url = Pointer_stringify(url);
        var id = RigEmscripten.workers.length;
        var info = {
            worker: new Worker(url),
            func: null,
            func_arg: null,
            buffer: 0,
            bufferSize: 0,
        };
        info.worker.onmessage = function info_worker_onmessage(msg) {
            if (ABORT) return;
            var info = RigEmscripten.workers[id];
            if (!info) return; // worker was destroyed meanwhile
            if (!info.func) return; // no callback or callback removed meanwhile
            // Don't trash our callback state if we expect additional calls.
            var data = msg.data['data'];
            if (data) {
                if (!data.byteLength) data = new Uint8Array(data);
                if (!info.buffer || info.bufferSize < data.length) {
                    if (info.buffer) _free(info.buffer);
                    info.bufferSize = data.length;
                    info.buffer = _malloc(data.length);
                }
                HEAPU8.set(data, info.buffer);
                info.func(info.buffer, data.length, info.func_arg);
            } else {
                info.func(0, 0, info.func_arg);
            }
        };
        RigEmscripten.workers.push(info);
        return id;
    },

    rig_emscripten_worker_set_main_onmessage: function(id, callback, arg) {
        var info = RigEmscripten.workers[id];
        if (!info) return; // worker was destroyed meanwhile
        info.func = Runtime.getFuncWrapper(callback, 'viii');
        info.func_arg = arg;
    },

    rig_emscripten_worker_destroy: function(id) {
        var info = RigEmscripten.workers[id];
        info.worker.terminate();
        if (info.buffer) _free(info.buffer);
        RigEmscripten.workers[id] = null;
    },

    rig_emscripten_worker_post: function(id, funcName, data, size) {
        Module['noExitRuntime'] = true; // should we only do this if there is a callback?

        funcName = Pointer_stringify(funcName);
        var info = RigEmscripten.workers[id];
        info.worker.postMessage({
            'funcName': funcName,
            'callbackId': -1,
            'data': data ? new Uint8Array({{{ makeHEAPView('U8', 'data', 'data + size') }}}) : 0 // XXX copy to a new typed array as a workaround for chrome bug 169705
        });
    },

    rig_emscripten_worker_post_to_main: function(data, size) {
        postMessage({
            'data': data ? new Uint8Array({{{ makeHEAPView('U8', 'data', 'data + size') }}}) : 0 // XXX copy to a new typed array as a workaround for chrome bug 169705
        });
    },
};

autoAddDeps(LibraryRigEmscripten, '$RigEmscripten');
mergeInto(LibraryManager.library, LibraryRigEmscripten);
