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

// functions used for Video Codec profiles

function update_hwaccel_details(form) {
    form.findField('hwaccel_details').setDisabled(!form.findField('hwaccel').getValue());
    update_deinterlace_vaapi_mode(form);
}

function update_deinterlace_vaapi_mode(form) {
    if (form.findField('deinterlace_vaapi_mode')) {
        const hwaccel = form.findField('hwaccel').getValue();
        const deinterlace = form.findField('deinterlace').getValue();
        const hwaccelDetails = form.findField('hwaccel_details').getValue(); // 0=AUTO, 1=VAAPI, 2=NVDEC, 3=MMAL

        const disableDeintVaapiMode = !hwaccel || !deinterlace || (hwaccelDetails !== 0 && hwaccelDetails !== 1);
        form.findField('deinterlace_vaapi_mode').setDisabled(disableDeintVaapiMode);
    }
}

function update_deinterlace_details(form) {
    if (form.findField('deinterlace_field_rate') && form.findField('deinterlace_enable_auto')) {
        form.findField('deinterlace_field_rate').setDisabled(!form.findField('deinterlace').getValue());
        form.findField('deinterlace_enable_auto').setDisabled(!form.findField('deinterlace').getValue());
    }
    update_deinterlace_vaapi_mode(form);
}

function enable_hwaccels_details(form) {
    if (form.findField('hwaccel_details') && form.findField('hwaccel')) {
        update_hwaccel_details(form);
        // on hwaccel change
        form.findField('hwaccel').on('check', function(checkbox, value) {
            update_hwaccel_details(form);
        });
        // on hwaccel_details change
        form.findField('hwaccel_details').on('select', function(combo, record, index) {
            update_deinterlace_vaapi_mode(form);
        });
    }
    if (form.findField('deinterlace')) {
        // on deinterlace change
        form.findField('deinterlace').on('check', function(checkbox, value) {
            update_deinterlace_details(form);
        });
    }
}

// functions used for VAAPI Video Codec profiles

function updateHWFilters(form) {
    if (form.findField('hw_denoise') && form.findField('hw_sharpness') && 
        form.findField('hwaccel_details') && form.findField('hwaccel')) 
    {
        form.findField('hw_denoise').setDisabled(!form.findField('hwaccel').getValue());
        form.findField('hw_sharpness').setDisabled(!form.findField('hwaccel').getValue());
        form.findField('hwaccel_details').setDisabled(!form.findField('hwaccel').getValue());
    }
    update_deinterlace_details(form);
}

function checkBFrameQuality(low_power_field, desired_b_depth_field, b_reference_field, quality_field, ui_value, uilp_value) {
    // enable max B frames based on ui (vainfo)
    let uibframe = 0;
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
    let uiquality = 0;
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
    if (form.findField('low_power') && form.findField('desired_b_depth') && 
        form.findField('b_reference') && form.findField('quality') && 
        form.findField('ui') && form.findField('uilp')) 
    {
        checkBFrameQuality( form.findField('low_power'), form.findField('desired_b_depth'), 
                            form.findField('b_reference'), form.findField('quality'),
                            form.findField('ui').getValue(), form.findField('uilp').getValue());
    }
}

function removeConstrains(form) {
    const arr_fields = ['bit_rate', 'max_bit_rate', 'buff_factor', 'bit_rate_scale_factor', 'qp', 'low_power', 'desired_b_depth', 'b_reference', 'quality'];
    for (const str_field of arr_fields)
        if (form.findField(str_field))
            form.findField(str_field).setDisabled(false);
}

function update_rc_settings(form, bit_rate_value, max_bit_rate_value, buff_factor_value, bit_rate_scale_factor_value, qp_value) {
    if (form.findField('bit_rate') && form.findField('max_bit_rate') && 
        form.findField('buff_factor') && form.findField('bit_rate_scale_factor') && 
        form.findField('qp'))
        {
        form.findField('bit_rate').setDisabled(bit_rate_value);
        form.findField('max_bit_rate').setDisabled(max_bit_rate_value);
        form.findField('buff_factor').setDisabled(buff_factor_value);
        form.findField('bit_rate_scale_factor').setDisabled(bit_rate_scale_factor_value);
        form.findField('qp').setDisabled(qp_value);
    }
}

function filter_based_on_rc(form) {
    if (form.findField('rc_mode')) {
        switch (form.findField('rc_mode').getValue()) {
            case -1:
            case 0:
                // for auto --> let the driver decide as requested by documentation
                update_rc_settings(form, false, false, false, false, false);
                break;
            case 1:
            case 4:
                // for constant quality: CQP and ICQ we use qp
                update_rc_settings(form, true, true, true, true, false);
                break;
            case 2:
                // for constant bitrate: CBR we use bitrate
                update_rc_settings(form, false, true, false, false, true);
                break;
            case 3:
                // for variable bitrate: VBR we use bitrate
                update_rc_settings(form, false, false, false, false, true);
                break;
            case 5:
                // for variable bitrate: QVBR we use bitrate + qp
                update_rc_settings(form, false, false, false, false, false);
                break;
            case 6:
                // for variable bitrate: AVBR we use bitrate
                update_rc_settings(form, false, false, true, false, true);
                break;
        }
    }
}

function update_intel_low_power(form) {
    if (form.findField('ui') && form.findField('uilp')){
        let ui_value = form.findField('ui').getValue();
        let uilp_value = form.findField('uilp').getValue();
        if (form.findField('low_power')){
            // enable low_power mode based on ui (vainfo)
            if ((ui_value & 0x1) && (uilp_value & 0x1))
                form.findField('low_power').setDisabled(false);
            else {
                form.findField('low_power').setDisabled(true);
                if (uilp_value & 0x1)
                    // only low power is available
                form.findField('low_power').setValue(true);
                if (ui_value & 0x1)
                    // only normal is available
                form.findField('low_power').setValue(false);
            }
            updateLowPower(form);
        }
    }
}

function updateFilters(form) {
    if (form.findField('platform')){
        switch (form.findField('platform').getValue()) {
            case 0:
                // Unconstrained --> will allow any combination of parameters (valid or invalid)
                // this mode is usefull fur future platform and for debugging.
                // no filter applied
                removeConstrains(form);
                break;
            case 1:
                // Intel
                update_intel_low_power(form);
                // filter based in rc_mode
                filter_based_on_rc(form);
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
}


function update_vaapi_ui(form) {
    // first time we have to call this manually
    updateFilters(form);
    updateHWFilters(form);
    
    // on platform change
    if (form.findField('platform'))
        form.findField('platform').on('select', function(combo, record, index) {
            updateFilters(form);
        });
    
    // on rc_mode change
    if (form.findField('rc_mode'))
        form.findField('rc_mode').on('select', function(combo, record, index) {
            updateFilters(form);
        });

    // on low_power change
    if (form.findField('low_power'))
        form.findField('low_power').on('check', function(checkbox, value) {
            updateLowPower(form);
        });

    // on hwaccel change
    if (form.findField('hwaccel'))
        form.findField('hwaccel').on('check', function(checkbox, value) {
            updateHWFilters(form);
        });

    // on hwaccel_details change
    form.findField('hwaccel_details').on('select', function(combo, record, index) {
        update_deinterlace_vaapi_mode(form);
    });

    // on deinterlace change
    if (form.findField('deinterlace'))
        form.findField('deinterlace').on('check', function(checkbox, value) {
            update_deinterlace_details(form);
        });

    // on desired_b_depth change
    if (form.findField('desired_b_depth'))
        form.findField('desired_b_depth').on('spin', function(spinner, direction, eOpts) {
            updateFilters(form);
        });

    // on quality change
    if (form.findField('quality'))
        form.findField('quality').on('spin', function(spinner, direction, eOpts) {
            updateFilters(form);
        });
}

var codec_profile_forms = {
    'default': function(form) {
        var name_field = form.findField('name');
        name_field.maxLength = 31; // TVH_NAME_LEN -1
    },

    'codec_profile_mpeg2video': function(form) {
        genericCBRvsVBR(form);
        enable_hwaccels_details(form);
    },

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

    //'codec_profile_vorbis': function(form) { 
    //
    //},

    //'codec_profile_flac': function(form) { 
    //
    //},

    'codec_profile_libx264': function(form) {
        genericCBRvsVBR(form);
        enable_hwaccels_details(form);
    },

    'codec_profile_libx265': function(form) {
        genericCBRvsVBR(form);
        enable_hwaccels_details(form);
    },

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
        enable_hwaccels_details(form);
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

    //'codec_profile_libopus': function(form) { 
    //
    //},

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
        update_vaapi_ui(form);
    },

    'codec_profile_vaapi_hevc': function(form) {
        update_vaapi_ui(form);
    },

    'codec_profile_vaapi_vp8': function(form) {
        update_vaapi_ui(form);
    },

    'codec_profile_vaapi_vp9': function(form) {
        update_vaapi_ui(form);
    },

    'codec_profile_nvenc_h264': function(form) { 
        enable_hwaccels_details(form);
    },

    'codec_profile_nvenc_hevc': function(form) { 
        enable_hwaccels_details(form);
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
