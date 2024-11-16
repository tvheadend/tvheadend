/*
 * Codec Profiles
 */


function genericCBRvsVBR(form) {
    function updateBitrate(cbr_f, vbr_f) {
        if (vbr_f.getValue() > 0) {
            cbr_f.setValue(0);
            cbr_f.setReadOnly(true);
        }
        else {
            cbr_f.setReadOnly(false);
        }
    }

    var cbr_field = form.findField('bit_rate');
    var vbr_field = form.findField('qp') || form.findField('crf') || form.findField('qscale');
    vbr_field.spinner.editable = false;
    updateBitrate(cbr_field, vbr_field);

    vbr_field.on('spin', function(spinner) {
        updateBitrate(cbr_field, spinner.field);
    });
}


var codec_profile_forms = {
    'default': function(form) {
        var name_field = form.findField('name');
        name_field.maxLength = 31; // TVH_NAME_LEN -1
    },

    'codec_profile_mpeg2video': genericCBRvsVBR,

    'codec_profile_mp2': function(form) {
        function makeData(list) {
            var data = [{'key': list[0],  'val': 'auto'}];
            for (var i = 1; i < list.length; i++) {
                var val = list[i];
                data.push({'key': val, 'val': val.toString()});
            }
            return data;
        }

        // see avpriv_mpa_bitrate_tab and avpriv_mpa_freq_tab
        // in ffmpeg-3.0.2/libavcodec/mpegaudiodata.c
        var hi_freqs = [0, 44100, 48000, 32000];
        var hi_rates = makeData([0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384]);
        var lo_rates = makeData([0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]);

        function updateBitrate(samplerate_f, bitrate_f) {
            var bitrates = hi_rates;
            if (hi_freqs.indexOf(samplerate_f.getValue()) === -1) {
                bitrates = lo_rates;
            }
            bitrate_f.store.loadData(bitrates);
        }

        var samplerate_field = form.findField('sample_rate');
        samplerate_field.forceSelection = true;
        var bitrate_field = form.findField('bit_rate');
        bitrate_field.forceSelection = true;
        updateBitrate(samplerate_field, bitrate_field);

        samplerate_field.on('select', function(combo, record, index) {
            updateBitrate(combo, bitrate_field);
            if (bitrate_field.store.data.keys.indexOf(bitrate_field.getValue()) === -1) {
                bitrate_field.setValue(0);
            }
        });
    },

    'codec_profile_aac': function(form) {
        /*
            name                           | value | nb. channels
            -----------------------------------------------------
            AV_CH_LAYOUT_MONO              | 4     | 1
            AV_CH_LAYOUT_STEREO            | 3     | 2
            AV_CH_LAYOUT_SURROUND          | 7     | 3
            AV_CH_LAYOUT_4POINT0           | 263   | 4
            AV_CH_LAYOUT_5POINT0_BACK      | 55    | 5
            AV_CH_LAYOUT_5POINT1_BACK      | 63    | 6
            AV_CH_LAYOUT_7POINT1           | 1599  | 8
        */
        var channels = [0, 4, 3, 7, 263, 55, 63, 1599]

        function updateBitrate(bitrate_f, quality_f, samplerate_f, layout_f) {
            if (quality_f.getValue() > 0) {
                bitrate_f.setValue(0);
                bitrate_f.setReadOnly(true);
            }
            else {
                var samplerate = samplerate_f.getValue() || 48000;
                var layout = layout_f.getValue() || 3; // AV_CH_LAYOUT_STEREO
                if (channels.indexOf(layout) >= 0) {
                    var max_bitrate = (6 * samplerate * channels.indexOf(layout)) / 1000;
                    bitrate_f.setMaxValue(max_bitrate);
                    if (bitrate_f.getValue() > max_bitrate) {
                        bitrate_f.setValue(max_bitrate);
                    }
                }
                bitrate_f.setReadOnly(false);
            }
        }

        var bitrate_field = form.findField('bit_rate');
        var quality_field = form.findField('qscale');
        quality_field.minValue = 0;
        quality_field.maxValue = 2.0;
        quality_field.spinner.incrementValue = 0.1;
        quality_field.spinner.editable = false;
        var samplerate_field = form.findField('sample_rate');
        samplerate_field.forceSelection = true;
        var layout_field = form.findField('channel_layout');
        layout_field.forceSelection = true;
        updateBitrate(bitrate_field, quality_field, samplerate_field, layout_field);

        quality_field.on('spin', function(spinner) {
            updateBitrate(bitrate_field, spinner.field, samplerate_field, layout_field);
        });

        samplerate_field.on('select', function(combo, record, index) {
            updateBitrate(bitrate_field, quality_field, combo, layout_field);
        });

        layout_field.on('select', function(combo, record, index) {
            updateBitrate(bitrate_field, quality_field, samplerate_field, combo);
        });
    },

    'codec_profile_libx264': genericCBRvsVBR,

    'codec_profile_libx265': genericCBRvsVBR,

    'codec_profile_libvpx': function(form) {
        function updateSpeed(quality_f, speed_f) {
            if (quality_f.getValue() == 1) { // VPX_DL_REALTIME
                speed_f.setMaxValue(15);
            }
            else {
                speed_f.setMaxValue(5);
            }
        }

        genericCBRvsVBR(form);
        var quality_field = form.findField('deadline');
        var speed_field = form.findField('cpu-used');
        updateSpeed(quality_field, speed_field);

        quality_field.on('select', function(combo, record, index) {
            updateSpeed(combo, speed_field);
            if (speed_field.getValue() > speed_field.maxValue) {
                speed_field.setValue(speed_field.maxValue);
            }
        });
    },

    'codec_profile_libtheora': genericCBRvsVBR,

    'codec_profile_libvorbis': genericCBRvsVBR,

    'codec_profile_libfdk_aac': function(form) {
        // TODO: max bitrate
        // max vbr = 5 for FF_PROFILE_UNKNOWN(-99) and FF_PROFILE_AAC_LOW(1)
        // max vbr = 3 for FF_PROFILE_AAC_HE(4) and FF_PROFILE_AAC_HE_V2(28)
        // vbr disabled for FF_PROFILE_AAC_LD(22) and FF_PROFILE_AAC_ELD(38)
        var max_vbr = [null, null, null, [4, 28], null, [-99, 1]];
        function updateVBR(profile_f, cbr_f, vbr_f) {
            var profile = profile_f.getValue();
            for (var i = 0; i < max_vbr.length; i++) {
                var profiles = max_vbr[i];
                if (profiles && profiles.indexOf(profile) !== -1) {
                    vbr_f.setMaxValue(i);
                    if (vbr_f.getValue() > i) {
                        vbr_f.setValue(i);
                    }
                    vbr_f.spinner.setReadOnly(false);
                    if (vbr_f.getValue() > 0) {
                        cbr_f.setValue(0);
                        cbr_f.setReadOnly(true);
                    }
                    return;
                }
            }
            vbr_f.setValue(0);
            vbr_f.spinner.setReadOnly(true);
            cbr_f.setReadOnly(false);
        }

        function updateSignaling(profile_f, eld_sbr_f, signaling_f) {
            if (profile_f.getValue() === 38) { // FF_PROFILE_AAC_ELD
                eld_sbr_f.setReadOnly(false);
                signaling_f.setReadOnly(false);
            }
            else {
                eld_sbr_f.setValue(0);
                eld_sbr_f.setReadOnly(true);
                signaling_f.setValue(-1);
                signaling_f.setReadOnly(true);
            }
        }

        function updateBitrate(cbr_f, vbr_f) {
            if (vbr_f.getValue() > 0) {
                cbr_f.setValue(0);
                cbr_f.setReadOnly(true);
            }
            else {
                cbr_f.setReadOnly(false);
            }
        }

        var eld_sbr_field = form.findField('eld_sbr');
        var signaling_field = form.findField('signaling');
        var cbr_field = form.findField('bit_rate');
        var vbr_field = form.findField('vbr');
        vbr_field.spinner.editable = false;
        var profile_field = form.findField('profile');
        updateVBR(profile_field, cbr_field, vbr_field);
        updateSignaling(profile_field, eld_sbr_field, signaling_field);
        updateBitrate(cbr_field, vbr_field);

        vbr_field.on('spin', function(spinner) {
            updateBitrate(cbr_field, spinner.field);
        });

        profile_field.on('select', function(combo, record, index) {
            updateVBR(combo, cbr_field, vbr_field);
            updateSignaling(combo, eld_sbr_field, signaling_field);
        });
    },

    'codec_profile_vaapi_h264': function(form) {
        function updateHWFilters(form) {
            var hwaccel_field = form.findField('hwaccel');
            form.findField('hw_denoise').setDisabled(!hwaccel_field.getValue());
            form.findField('hw_sharpness').setDisabled(!hwaccel_field.getValue());
        }

        function checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field) {
            // enable max B frames based on ui (vainfo)
            var uibframe = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uibframe = (uilp_value & 0xe) >> 1
            }
            else {
                // low power is disabled
                uibframe = (ui_value & 0xe) >> 1
            }
            // messagebox if max B grames > VAINFO.max_b_frames
            if (uibframe) {
                if (desired_b_depth_field.getValue() > uibframe)
                    alert('VAINFO maximum B fames is: ' + uibframe)
                desired_b_depth_field.setDisabled(false);
                b_reference_field.setDisabled(false);
            }
            else {
                desired_b_depth_field.setDisabled(true);
                b_reference_field.setDisabled(true);
            }
            // enable Quality based on ui (vainfo)
            var uiquality = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uiquality = (uilp_value & 0xf0) >> 4
            }
            else {
                // low power is disabled
                uiquality = (ui_value & 0xf0) >> 4
            }
            // messagebox if Qality > VAINFO.quality
            if (uiquality > 1) {
                if (quality_field.getValue() > uiquality)
                    alert('VAINFO maximum Quality is: ' + uiquality)
            }
            else {
                quality_field.setValue(-1);
                quality_field.setDisabled(true);
            }
        }

        function updateLowPower(form) {
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');
            
            checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
        }

        function removeConstrains(form) {
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            bit_rate_field.setDisabled(false);
            max_bit_rate_field.setDisabled(false);
            buff_factor_field.setDisabled(false);
            bit_rate_scale_factor_field.setDisabled(false);
            qp_field.setDisabled(false);
            low_power_field.setDisabled(false);
            desired_b_depth_field.setDisabled(false);
            b_reference_field.setDisabled(false);
            quality_field.setDisabled(false);
        }

        function updateFilters(form, ui_value, uilp_value) {
            var platform_field = form.findField('platform');
            var rc_mode_field = form.findField('rc_mode');
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            var platform = platform_field.getValue();
            switch (platform) {
                case 0:
                    // Unconstrained --> will allow any combination of parameters (valid or invalid)
                    // this mode is usefull fur future platform and for debugging.
                    // no filter applied
                    removeConstrains(form);
                    break;
                case 1:
                    // Intel
                    // enable low_power mode based on ui (vainfo)
                    if ((ui_value & 0x1) && (uilp_value & 0x1))
                        low_power_field.setDisabled(false);
                    else {
                        low_power_field.setDisabled(true);
                        if (uilp_value & 0x1)
                            // only low power is available
                            low_power_field.setValue(true);
                        if (ui_value & 0x1)
                            // only normal is available
                            low_power_field.setValue(false);
                    }
                    checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
                    // filter based in rc_mode
                    var rc_mode = rc_mode_field.getValue();
                    switch (rc_mode) {
                        case -1:
                        case 0:
                            // for auto --> let the driver decide as requested by documentation
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 1:
                        case 4:
                            // for constant quality: CQP and ICQ we use qp
                            bit_rate_field.setDisabled(true);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(true);
                            qp_field.setDisabled(false);
                            break;
                        case 2:
                            // for constant bitrate: CBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 3:
                            // for variable bitrate: VBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 5:
                            // for variable bitrate: QVBR we use bitrate + qp
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 6:
                            // for variable bitrate: AVBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                    }
                    break;
                case 2:
                    // AMD --> will allow any combination of parameters
                    // I am unable to confirm this platform because I don't have the HW
                    // Is only going to override bf to 0 (as highlited by the previous implementation)
                    // NOTE: filters to be added later
                    removeConstrains(form);
                    break;
                default:
            }
        }

        var platform_field = form.findField('platform');
        var rc_mode_field = form.findField('rc_mode');
        var low_power_field = form.findField('low_power');
        var hwaccel_field = form.findField('hwaccel');
        var ui_field = form.findField('ui');
        var uilp_field = form.findField('uilp');
        var desired_b_depth_field = form.findField('desired_b_depth');
        var quality_field = form.findField('quality');
        var ui_value = ui_field.getValue();
        var uilp_value = uilp_field.getValue();
        
        // first time we have to call this manually
        updateFilters(form, ui_value, uilp_value);
        updateHWFilters(form);
        
        // on platform change
        platform_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on rc_mode change
        rc_mode_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on low_power change
        low_power_field.on('check', function(checkbox, value) {
            updateLowPower(form);
        });
        // on hwaccel change
        hwaccel_field.on('check', function(checkbox, value) {
            updateHWFilters(form);
        });
        // on desired_b_depth change
        desired_b_depth_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on quality change
        quality_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
    },

    'codec_profile_vaapi_hevc': function(form) {
        function updateHWFilters(form) {
            var hwaccel_field = form.findField('hwaccel');
            form.findField('hw_denoise').setDisabled(!hwaccel_field.getValue());
            form.findField('hw_sharpness').setDisabled(!hwaccel_field.getValue());
        }

        function checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field) {
            // enable max B frames based on ui (vainfo)
            var uibframe = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uibframe = (uilp_value & 0xe) >> 1
            }
            else {
                // low power is disabled
                uibframe = (ui_value & 0xe) >> 1
            }
            // messagebox if max B grames > VAINFO.max_b_frames
            if (uibframe) {
                if (desired_b_depth_field.getValue() > uibframe)
                    alert('VAINFO maximum B fames is: ' + uibframe)
                desired_b_depth_field.setDisabled(false);
                b_reference_field.setDisabled(false);
            }
            else {
                desired_b_depth_field.setDisabled(true);
                b_reference_field.setDisabled(true);
            }
            // enable Quality based on ui (vainfo)
            var uiquality = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uiquality = (uilp_value & 0xf0) >> 4
            }
            else {
                // low power is disabled
                uiquality = (ui_value & 0xf0) >> 4
            }
            // messagebox if Qality > VAINFO.quality
            if (uiquality > 1) {
                if (quality_field.getValue() > uiquality)
                    alert('VAINFO maximum Quality is: ' + uiquality)
            }
            else {
                quality_field.setValue(-1);
                quality_field.setDisabled(true);
            }
        }

        function updateLowPower(form) {
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');
            
            checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
        }

        function removeConstrains(form) {
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            bit_rate_field.setDisabled(false);
            max_bit_rate_field.setDisabled(false);
            buff_factor_field.setDisabled(false);
            bit_rate_scale_factor_field.setDisabled(false);
            qp_field.setDisabled(false);
            low_power_field.setDisabled(false);
            desired_b_depth_field.setDisabled(false);
            b_reference_field.setDisabled(false);
            quality_field.setDisabled(false);
        }

        function updateFilters(form, ui_value, uilp_value) {
            var platform_field = form.findField('platform');
            var rc_mode_field = form.findField('rc_mode');
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            var platform = platform_field.getValue();
            switch (platform) {
                case 0:
                    // Unconstrained --> will allow any combination of parameters (valid or invalid)
                    // this mode is usefull fur future platform and for debugging.
                    // no filter applied
                    removeConstrains(form);
                    break;
                case 1:
                    // Intel
                    // enable low_power mode based on ui (vainfo)
                    if ((ui_value & 0x1) && (uilp_value & 0x1))
                        low_power_field.setDisabled(false);
                    else {
                        low_power_field.setDisabled(true);
                        if (uilp_value & 0x1)
                            // only low power is available
                            low_power_field.setValue(true);
                        if (ui_value & 0x1)
                            // only normal is available
                            low_power_field.setValue(false);
                    }
                    checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
                    // filter based in rc_mode
                    var rc_mode = rc_mode_field.getValue();
                    switch (rc_mode) {
                        case -1:
                        case 0:
                            // for auto --> let the driver decide as requested by documentation
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 1:
                        case 4:
                            // for constant quality: CQP and ICQ we use qp
                            bit_rate_field.setDisabled(true);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(true);
                            qp_field.setDisabled(false);
                            break;
                        case 2:
                            // for constant bitrate: CBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 3:
                            // for variable bitrate: VBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 5:
                            // for variable bitrate: QVBR we use bitrate + qp
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 6:
                            // for variable bitrate: AVBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                    }
                    break;
                case 2:
                    // AMD --> will allow any combination of parameters
                    // I am unable to confirm this platform because I don't have the HW
                    // Is only going to override bf to 0 (as highlited by the previous implementation)
                    // NOTE: filters to be added later
                    removeConstrains(form);
                    break;
                default:
            }
        }

        var platform_field = form.findField('platform');
        var rc_mode_field = form.findField('rc_mode');
        var low_power_field = form.findField('low_power');
        var hwaccel_field = form.findField('hwaccel');
        var ui_field = form.findField('ui');
        var uilp_field = form.findField('uilp');
        var desired_b_depth_field = form.findField('desired_b_depth');
        var quality_field = form.findField('quality');
        var ui_value = ui_field.getValue();
        var uilp_value = uilp_field.getValue();
        
        // first time we have to call this manually
        updateFilters(form, ui_value, uilp_value);
        updateHWFilters(form);
        
        // on platform change
        platform_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on rc_mode change
        rc_mode_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on low_power change
        low_power_field.on('check', function(checkbox, value) {
            updateLowPower(form);
        });
        // on hwaccel change
        hwaccel_field.on('check', function(checkbox, value) {
            updateHWFilters(form);
        });
        // on desired_b_depth change
        desired_b_depth_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on quality change
        quality_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
    },

    'codec_profile_vaapi_vp8': function(form) {
        function updateHWFilters(form) {
            var hwaccel_field = form.findField('hwaccel');
            form.findField('hw_denoise').setDisabled(!hwaccel_field.getValue());
            form.findField('hw_sharpness').setDisabled(!hwaccel_field.getValue());
        }

        function checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field) {
            // enable max B frames based on ui (vainfo)
            var uibframe = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uibframe = (uilp_value & 0xe) >> 1
            }
            else {
                // low power is disabled
                uibframe = (ui_value & 0xe) >> 1
            }
            // messagebox if max B grames > VAINFO.max_b_frames
            if (uibframe) {
                if (desired_b_depth_field.getValue() > uibframe)
                    alert('VAINFO maximum B fames is: ' + uibframe)
                desired_b_depth_field.setDisabled(false);
                b_reference_field.setDisabled(false);
            }
            else {
                desired_b_depth_field.setDisabled(true);
                b_reference_field.setDisabled(true);
            }
            // enable Quality based on ui (vainfo)
            var uiquality = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uiquality = (uilp_value & 0xf0) >> 4
            }
            else {
                // low power is disabled
                uiquality = (ui_value & 0xf0) >> 4
            }
            // messagebox if Qality > VAINFO.quality
            if (uiquality > 1) {
                if (quality_field.getValue() > uiquality)
                    alert('VAINFO maximum Quality is: ' + uiquality)
            }
            else {
                quality_field.setValue(-1);
                quality_field.setDisabled(true);
            }
        }

        function updateLowPower(form) {
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');
            
            checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
        }

        function removeConstrains(form) {
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            bit_rate_field.setDisabled(false);
            max_bit_rate_field.setDisabled(false);
            buff_factor_field.setDisabled(false);
            bit_rate_scale_factor_field.setDisabled(false);
            qp_field.setDisabled(false);
            low_power_field.setDisabled(false);
            desired_b_depth_field.setDisabled(false);
            b_reference_field.setDisabled(false);
            quality_field.setDisabled(false);
        }

        function updateFilters(form, ui_value, uilp_value) {
            var platform_field = form.findField('platform');
            var rc_mode_field = form.findField('rc_mode');
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            var platform = platform_field.getValue();
            switch (platform) {
                case 0:
                    // Unconstrained --> will allow any combination of parameters (valid or invalid)
                    // this mode is usefull fur future platform and for debugging.
                    // no filter applied
                    removeConstrains(form);
                    break;
                case 1:
                    // Intel
                    // enable low_power mode based on ui (vainfo)
                    if ((ui_value & 0x1) && (uilp_value & 0x1))
                        low_power_field.setDisabled(false);
                    else {
                        low_power_field.setDisabled(true);
                        if (uilp_value & 0x1)
                            // only low power is available
                            low_power_field.setValue(true);
                        if (ui_value & 0x1)
                            // only normal is available
                            low_power_field.setValue(false);
                    }
                    checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
                    // filter based in rc_mode
                    var rc_mode = rc_mode_field.getValue();
                    switch (rc_mode) {
                        case -1:
                        case 0:
                            // for auto --> let the driver decide as requested by documentation
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 1:
                        case 4:
                            // for constant quality: CQP and ICQ we use qp
                            bit_rate_field.setDisabled(true);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(true);
                            qp_field.setDisabled(false);
                            break;
                        case 2:
                            // for constant bitrate: CBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 3:
                            // for variable bitrate: VBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 5:
                            // for variable bitrate: QVBR we use bitrate + qp
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 6:
                            // for variable bitrate: AVBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                    }
                    break;
                case 2:
                    // AMD --> will allow any combination of parameters
                    // I am unable to confirm this platform because I don't have the HW
                    // Is only going to override bf to 0 (as highlited by the previous implementation)
                    // NOTE: filters to be added later
                    removeConstrains(form);
                    break;
                default:
            }
        }

        var platform_field = form.findField('platform');
        var rc_mode_field = form.findField('rc_mode');
        var low_power_field = form.findField('low_power');
        var hwaccel_field = form.findField('hwaccel');
        var ui_field = form.findField('ui');
        var uilp_field = form.findField('uilp');
        var desired_b_depth_field = form.findField('desired_b_depth');
        var quality_field = form.findField('quality');
        var ui_value = ui_field.getValue();
        var uilp_value = uilp_field.getValue();
        
        // first time we have to call this manually
        updateFilters(form, ui_value, uilp_value);
        updateHWFilters(form);
        
        // on platform change
        platform_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on rc_mode change
        rc_mode_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on low_power change
        low_power_field.on('check', function(checkbox, value) {
            updateLowPower(form);
        });
        // on hwaccel change
        hwaccel_field.on('check', function(checkbox, value) {
            updateHWFilters(form);
        });
        // on desired_b_depth change
        desired_b_depth_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on quality change
        quality_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
    },

    'codec_profile_vaapi_vp9': function(form) {
        function updateHWFilters(form) {
            var hwaccel_field = form.findField('hwaccel');
            form.findField('hw_denoise').setDisabled(!hwaccel_field.getValue());
            form.findField('hw_sharpness').setDisabled(!hwaccel_field.getValue());
        }

        function checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field) {
            // enable max B frames based on ui (vainfo)
            var uibframe = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uibframe = (uilp_value & 0xe) >> 1
            }
            else {
                // low power is disabled
                uibframe = (ui_value & 0xe) >> 1
            }
            // messagebox if max B grames > VAINFO.max_b_frames
            if (uibframe) {
                if (desired_b_depth_field.getValue() > uibframe)
                    alert('VAINFO maximum B fames is: ' + uibframe)
                desired_b_depth_field.setDisabled(false);
                b_reference_field.setDisabled(false);
            }
            else {
                desired_b_depth_field.setDisabled(true);
                b_reference_field.setDisabled(true);
            }
            // enable Quality based on ui (vainfo)
            var uiquality = 0;
            if (low_power_field.getValue()) {
                // low power is enabled
                uiquality = (uilp_value & 0xf0) >> 4
            }
            else {
                // low power is disabled
                uiquality = (ui_value & 0xf0) >> 4
            }
            // messagebox if Qality > VAINFO.quality
            if (uiquality > 1) {
                if (quality_field.getValue() > uiquality)
                    alert('VAINFO maximum Quality is: ' + uiquality)
            }
            else {
                quality_field.setValue(-1);
                quality_field.setDisabled(true);
            }
        }

        function updateLowPower(form) {
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');
            
            checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
        }

        function removeConstrains(form) {
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            bit_rate_field.setDisabled(false);
            max_bit_rate_field.setDisabled(false);
            buff_factor_field.setDisabled(false);
            bit_rate_scale_factor_field.setDisabled(false);
            qp_field.setDisabled(false);
            low_power_field.setDisabled(false);
            desired_b_depth_field.setDisabled(false);
            b_reference_field.setDisabled(false);
            quality_field.setDisabled(false);
        }

        function updateFilters(form, ui_value, uilp_value) {
            var platform_field = form.findField('platform');
            var rc_mode_field = form.findField('rc_mode');
            var bit_rate_field = form.findField('bit_rate');
            var max_bit_rate_field = form.findField('max_bit_rate');
            var buff_factor_field = form.findField('buff_factor');
            var bit_rate_scale_factor_field = form.findField('bit_rate_scale_factor');
            var qp_field = form.findField('qp');
            var low_power_field = form.findField('low_power');
            var desired_b_depth_field = form.findField('desired_b_depth');
            var b_reference_field = form.findField('b_reference');
            var quality_field = form.findField('quality');

            var platform = platform_field.getValue();
            switch (platform) {
                case 0:
                    // Unconstrained --> will allow any combination of parameters (valid or invalid)
                    // this mode is usefull fur future platform and for debugging.
                    // no filter applied
                    removeConstrains(form);
                    break;
                case 1:
                    // Intel
                    // enable low_power mode based on ui (vainfo)
                    if ((ui_value & 0x1) && (uilp_value & 0x1))
                        low_power_field.setDisabled(false);
                    else {
                        low_power_field.setDisabled(true);
                        if (uilp_value & 0x1)
                            // only low power is available
                            low_power_field.setValue(true);
                        if (ui_value & 0x1)
                            // only normal is available
                            low_power_field.setValue(false);
                    }
                    checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field);
                    // filter based in rc_mode
                    var rc_mode = rc_mode_field.getValue();
                    switch (rc_mode) {
                        case -1:
                        case 0:
                            // for auto --> let the driver decide as requested by documentation
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 1:
                        case 4:
                            // for constant quality: CQP and ICQ we use qp
                            bit_rate_field.setDisabled(true);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(true);
                            qp_field.setDisabled(false);
                            break;
                        case 2:
                            // for constant bitrate: CBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(true);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 3:
                            // for variable bitrate: VBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                        case 5:
                            // for variable bitrate: QVBR we use bitrate + qp
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(false);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(false);
                            break;
                        case 6:
                            // for variable bitrate: AVBR we use bitrate
                            bit_rate_field.setDisabled(false);
                            max_bit_rate_field.setDisabled(false);
                            buff_factor_field.setDisabled(true);
                            bit_rate_scale_factor_field.setDisabled(false);
                            qp_field.setDisabled(true);
                            break;
                    }
                    break;
                case 2:
                    // AMD --> will allow any combination of parameters
                    // I am unable to confirm this platform because I don't have the HW
                    // Is only going to override bf to 0 (as highlited by the previous implementation)
                    // NOTE: filters to be added later
                    removeConstrains(form);
                    break;
                default:
            }
        }

        var platform_field = form.findField('platform');
        var rc_mode_field = form.findField('rc_mode');
        var low_power_field = form.findField('low_power');
        var hwaccel_field = form.findField('hwaccel');
        var ui_field = form.findField('ui');
        var uilp_field = form.findField('uilp');
        var desired_b_depth_field = form.findField('desired_b_depth');
        var quality_field = form.findField('quality');
        var ui_value = ui_field.getValue();
        var uilp_value = uilp_field.getValue();
        
        // first time we have to call this manually
        updateFilters(form, ui_value, uilp_value);
        updateHWFilters(form);
        
        // on platform change
        platform_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on rc_mode change
        rc_mode_field.on('select', function(combo, record, index) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on low_power change
        low_power_field.on('check', function(checkbox, value) {
            updateLowPower(form);
        });
        // on hwaccel change
        hwaccel_field.on('check', function(checkbox, value) {
            updateHWFilters(form);
        });
        // on desired_b_depth change
        desired_b_depth_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
        // on quality change
        quality_field.on('spin', function(spinner, direction, eOpts) {
            updateFilters(form, ui_value, uilp_value);
        });
    }
};


tvheadend.codec_tab = function(panel)
{
    if (tvheadend.capabilities.indexOf('libav') !== -1) {
        var actions = new Ext.ux.grid.RowActions({
            id: 'status',
            header: '',
            width: 45,
            actions: [ { iconIndex: 'status' } ],
            destroy: function() {}
        });

        if (!tvheadend.codec_list) {
            tvheadend.codec_list = new Ext.data.JsonStore({
                url: 'api/codec/list',
                root: 'entries',
                fields: ['name', 'title'],
                id: 'name',
                autoLoad: true
            });
        }

        tvheadend.idnode_form_grid(panel, {
            url: 'api/codec_profile',
            clazz: 'codec_profile',
            comet: 'codec_profile',
            titleS: _('Codec Profile'),
            titleP: _('Codec Profiles'),
            titleC: _('Codec Profile Name'),
            iconCls: 'option',
            key: 'uuid',
            val: 'title',
            fields: ['uuid', 'title', 'status'],
            list: {url: 'api/codec_profile/list', params: {}},
            lcol: [actions],
            plugins: [actions],
            add: {
                url: 'api/codec_profile',
                titleS: _('Codec Profile'),
                select: {
                    label: _('Codec'),
                    store: tvheadend.codec_list,
                    fullRecord: true,
                    displayField: 'title',
                    valueField: 'name',
                    formField: 'codec_name',
                    list: '-type,enabled'
                },
                create: {}
            },
            del: true,
            forms: codec_profile_forms
        });
    }
};
