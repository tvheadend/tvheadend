tvheadend.timeshift = function(panel, index) {

    function onchange(form, field, nval, oval) {
       var f = form.getForm();
       var unlperiod = f.findField('unlimited_period');
       var unlsize = f.findField('unlimited_size');
       var ramonly = f.findField('ram_only');
       var maxperiod = f.findField('max_period');
       var maxsize = f.findField('max_size');
       if (unlperiod !== null)
           maxperiod.setDisabled(unlperiod.getValue());
       if (unlsize !== null && ramonly !== null)
           maxsize.setDisabled(unlsize.getValue() || ramonly.getValue());
    }

    tvheadend.idnode_simple(panel, {
        url: 'api/timeshift/config',
        title: _('Timeshift'),
        iconCls: 'timeshift',
        tabIndex: index,
        comet: 'timeshift',
        labelWidth: 220,
        width: 570,
        onchange: onchange
    });

};
