(function() {
    var type_impls = Object.fromEntries([["alsa_sys",[]],["freetype",[]],["portaudio_sys",[]]]);
    if (window.register_type_impls) {
        window.register_type_impls(type_impls);
    } else {
        window.pending_type_impls = type_impls;
    }
})()
//{"start":55,"fragment_lengths":[15,16,21]}