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
            AV_CH_LAYOUT_7POINT1_WIDE_BACK | 255   | 8
        */
        var channels = [0, 4, 3, 7, 263, 55, 63, -1, 255]

        function updateBitrate(bitrate_f, quality_f, samplerate_f, layout_f) {
            if (quality_f.getValue() > 0) {
                bitrate_f.setValue(0);
                bitrate_f.setReadOnly(true);
            }
            else {
                var samplerate = samplerate_f.getValue() || 48000;
                var layout = layout_f.getValue() || 3; // AV_CH_LAYOUT_STEREO
                var max_bitrate = (6 * samplerate * channels.indexOf(layout)) / 1000;
                bitrate_f.setMaxValue(max_bitrate);
                if (bitrate_f.getValue() > max_bitrate) {
                    bitrate_f.setValue(max_bitrate);
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

    'codec_profile_vaapi_h264': genericCBRvsVBR,

    'codec_profile_vaapi_hevc': genericCBRvsVBR
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
