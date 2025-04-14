/*
 * Stream Profiles
 */

let stream_profile_forms = {
    'default': function(form) {
        let name_field = form.findField('name');
        name_field.maxLength = 31; // TVH_NAME_LEN -1
    },

    'profile-transcode': function(form) {
        function updateMPEGTS(form) {
            // enable settings for MPEG-TS/av-lib
            // /src/muxer.h:32  MC_MPEGTS      = 2,
            if (form.findField('container').getValue() == 2) {
                form.findField('sid').setDisabled(false);
                form.findField('rewrite_pmt').setDisabled(false);
                form.findField('rewrite_nit').setDisabled(false);
            }
            else {
                form.findField('sid').setDisabled(true);
                form.findField('rewrite_pmt').setDisabled(true);
                form.findField('rewrite_nit').setDisabled(true);
            }
        }

        // update when form is loaded
        updateMPEGTS(form);
        
        // on container change
        form.findField('container').on('select', function(combo, record, index) {
            updateMPEGTS(form);
        });
    }
};

tvheadend.profile_tab = function(panel)
{
    if (!tvheadend.profile_builders) {
        tvheadend.profile_builders = new Ext.data.JsonStore({
            url: 'api/profile/builders',
            root: 'entries',
            fields: ['class', 'caption', 'order', 'groups', 'params'],
            id: 'class',
            autoLoad: true
        });
    }

    var list = '-class';

    tvheadend.idnode_form_grid(panel, {
        url: 'api/profile',
        clazz: 'profile',
        comet: 'profile',
        titleS: _('Stream Profile'),
        titleP: _('Stream Profiles'),
        titleC: _('Stream Profile Name'),
        iconCls: 'film',
        edit: { params: { list: list } },
        add: {
            url: 'api/profile',
            titleS: _('Stream Profile'),
            select: {
                label: _('Type'),
                store: tvheadend.profile_builders,
                fullRecord: true,
                displayField: 'caption',
                valueField: 'class',
                list: list
            },
            create: { }
        },
        del: true,
        forms: stream_profile_forms
    });
};
