tvheadend.tvhlog = function(panel, index) {

    function onchange(form, field, nval, oval) {
       var f = form.getForm();
       var enable_syslog = f.findField('enable_syslog');
       var debug_syslog = f.findField('syslog');
       if (debug_syslog.cbEl)
         debug_syslog.setDisabled(!enable_syslog.getValue() || enable_syslog.disabled);
       var trace = f.findField('trace');
       var tracesubs = f.findField('tracesubs');
       if (tracesubs.cbEl)
         tracesubs.setDisabled(!trace.getValue() || trace.disabled);
    }

    tvheadend.idnode_simple(panel, {
        url: 'api/tvhlog/config',
        title: _('Debugging'),
        iconCls: 'debug',
        tabIndex: index,
        comet: 'tvhlog_conf',
        labelWidth: 180,
        width: 530,
        onchange: onchange,
        saveText: _("Apply configuration (run-time only)"),
        saveTooltip: _('Apply any changes made below to the run-time configuration.') + '<br/>' +
                     _('They will be lost when the application next restarts.'),
        help: function() {
            new tvheadend.help(_('Debug Configuration'), 'config_debugging.html');
        }
    });

};
