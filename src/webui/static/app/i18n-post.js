/*
 * Tvheadend specific locale definitions for ExtJS
 */

Ext.UpdateManager.defaults.indicatorText = '<div class="loading-indicator">' + _('Loading...') + '</div>';

if(Ext.data.Types)
  Ext.data.Types.stripRe = /[\$,%]/g;

if(Ext.DataView)
  Ext.DataView.prototype.emptyText = "";

if(Ext.grid.GridPanel)
  Ext.grid.GridPanel.prototype.ddText = _("{0} selected row{1}");

if(Ext.LoadMask)
  Ext.LoadMask.prototype.msg = _("Loading...");

Date.monthNames = [
  _("January"),
  _("February"),
  _("March"),
  _("April"),
  _("May"),
  _("June"),
  _("July"),
  _("August"),
  _("September"),
  _("October"),
  _("November"),
  _("December")
];

Date.monthNames2 = [
  _("Jan"),
  _("Feb"),
  _("Mar"),
  _("Apr"),
  _("May"),
  _("Jun"),
  _("Jul"),
  _("Aug"),
  _("Sep"),
  _("Oct"),
  _("Nov"),
  _("Dec")
];

Date.shortMonthNames = {};

for (__i = 0; __i < 12; __i++)
  Date.shortMonthNames[Date.monthNames[__i]] = Date.monthNames2[__i];

Date.getShortMonthName = function(month) {
  return Date.shortMonthNames[Date.monthNames[month]];
};

Date.monthNumbers2 = [
  parseInt(_('0 #monthNumber')),
  parseInt(_('1 #monthNumber')),
  parseInt(_('2 #monthNumber')),
  parseInt(_('3 #monthNumber')),
  parseInt(_('4 #monthNumber')),
  parseInt(_('5 #monthNumber')),
  parseInt(_('6 #monthNumber')),
  parseInt(_('7 #monthNumber')),
  parseInt(_('8 #monthNumber')),
  parseInt(_('9 #monthNumber')),
  parseInt(_('10 #monthNumber')),
  parseInt(_('11 #monthNumber'))
];

Date.monthNumbers = {};

for (__i = 0; __i < 12; __i++)
  Date.monthNumbers[Date.monthNames2[__i]] = Date.monthNumbers2[__i];

Date.getMonthNumber = function(name) {
  return Date.monthNumbers[name.substring(0, 1).toUpperCase() + name.substring(1).toLowerCase()];
};

Date.dayNames = [
  _("Sunday"),
  _("Monday"),
  _("Tuesday"),
  _("Wednesday"),
  _("Thursday"),
  _("Friday"),
  _("Saturday")
];

Date.dayNames2 = [
  _("Sun"),
  _("Mon"),
  _("Tue"),
  _("Wed"),
  _("Thu"),
  _("Fri"),
  _("Sat")
];

Date.getShortDayName = function(day) {
  return Date.dayNames2[day];
};

Date.parseCodes.S.s = _("(?:st|nd|rd|th)#parseCodes.S.s").split('#')[0];

if(Ext.MessageBox)
  Ext.MessageBox.buttonText = {
    ok     : _("OK"),
    cancel : _("Cancel"),
    yes    : _("Yes"),
    no     : _("No")
  };

if(Ext.util.Format)
  Ext.util.Format.date = function(v, format){
    if(!v) return "";
    if(!(v instanceof Date)) v = new Date(Date.parse(v));
    return v.dateFormat(format || _("m/d/Y#Format.date").split('#')[0]);
  };

if(Ext.DatePicker)
  Ext.apply(Ext.DatePicker.prototype, {
    todayText         : _("Today"),
    minText           : _("This date is before the minimum date"),
    maxText           : _("This date is after the maximum date"),
    disabledDaysText  : "",
    disabledDatesText : "",
    monthNames        : Date.monthNames,
    dayNames          : Date.dayNames,
    nextText          : _('Next Month (Control+Right)'),
    prevText          : _('Previous Month (Control+Left)'),
    monthYearText     : _('Choose a month (Control+Up/Down to move years)'),
    todayTip          : _("{0} (Spacebar)"),
    format            : _("m/d/y#DatePicker").split('#')[0],
    okText            : _("&#160;OK&#160;"),
    cancelText        : _("Cancel"),
    startDay          : 0
  });

if(Ext.PagingToolbar)
  Ext.apply(Ext.PagingToolbar.prototype, {
    beforePageText : _("Page"),
    afterPageText  : _("of {0}"),
    firstText      : _("First Page"),
    prevText       : _("Previous Page"),
    nextText       : _("Next Page"),
    lastText       : _("Last Page"),
    refreshText    : _("Refresh"),
    displayMsg     : _("Displaying {0} - {1} of {2}"),
    emptyMsg       : _('No data to display')
  });

if(Ext.form.BasicForm)
    Ext.form.BasicForm.prototype.waitTitle = _("Please Wait...");

if(Ext.form.Field)
  Ext.form.Field.prototype.invalidText = _("The value in this field is invalid");

if(Ext.form.TextField)
  Ext.apply(Ext.form.TextField.prototype, {
    minLengthText : _("The minimum length for this field is {0}"),
    maxLengthText : _("The maximum length for this field is {0}"),
    blankText     : _("This field is required"),
    regexText     : "",
    emptyText     : null
  });

if(Ext.form.NumberField)
  Ext.apply(Ext.form.NumberField.prototype, {
    decimalSeparator : ".",
    decimalPrecision : 2,
    maxText : _("The maximum value for this field is {0}"),
    nanText : _("{0} is not a valid number")
  });

if(Ext.form.DateField)
  Ext.apply(Ext.form.DateField.prototype, {
    disabledDaysText  : _("Disabled"),
    disabledDatesText : _("Disabled"),
    minText           : _("The date in this field must be after {0}"),
    maxText           : _("The date in this field must be before {0}"),
    invalidText       : _("{0} is not a valid date - it must be in the format {1}"),
    format            : _("m/d/y#DateField").split('#')[0],
    altFormats        : _("m/d/Y|m-d-y|m-d-Y|m/d|m-d|md|mdy|mdY|d|Y-m-d#DateField").split('#')[0],
    startDay          : 0
  });

if(Ext.form.ComboBox)
  Ext.apply(Ext.form.ComboBox.prototype, {
    loadingText       : _("Loading..."),
    valueNotFoundText : undefined
  });

if(Ext.grid.GridView)
  Ext.apply(Ext.grid.GridView.prototype, {
    sortAscText  : _("Sort Ascending"),
    sortDescText : _("Sort Descending"),
    columnsText  : _("Columns")
  });

if(Ext.grid.GroupingView)
  Ext.apply(Ext.grid.GroupingView.prototype, {
    emptyGroupText : _('(None)'),
    groupByText    : _('Group By This Field'),
    showGroupsText : _('Show in Groups')
  });

if(Ext.grid.BooleanColumn)
  Ext.apply(Ext.grid.BooleanColumn.prototype, {
    trueText  : _("true"),
    falseText : _("false"),
    undefinedText: '&#160;'
  });

if(Ext.grid.NumberColumn)
  Ext.apply(Ext.grid.NumberColumn.prototype, {
    format : _('0,000.00#NumberColumn').split('#')[0]
  });

if(Ext.grid.DateColumn)
  Ext.apply(Ext.grid.DateColumn.prototype, {
    format : _('m/d/Y#DateColumn').split('#')[0]
  });

if(Ext.form.TimeField)
  Ext.apply(Ext.form.TimeField.prototype, {
    minText : _("The time in this field must be equal to or after {0}"),
    maxText : _("The time in this field must be equal to or before {0}"),
    invalidText : _("{0} is not a valid time"),
    format : _("g:i A#TimeField").split('#')[0],
    altFormats : _("g:ia|g:iA|g:i a|g:i A|h:i|g:i|H:i|ga|ha|gA|h a|g a|g A|gi|hi|gia|hia|g|H#TimeField").split('#')[0]
  });

if(Ext.form.CheckboxGroup)
  Ext.apply(Ext.form.CheckboxGroup.prototype, {
    blankText : _("You must select at least one item in this group")
  });

if(Ext.form.RadioGroup)
  Ext.apply(Ext.form.RadioGroup.prototype, {
    blankText : _("You must select one item in this group")
  });

if(Ext.ux.grid.GridFilters)
  Ext.apply(Ext.ux.grid.GridFilters.prototype, {
    menuFilterText: _('Filters'),
  });

if(Ext.ux.grid.filter.BoolFilter)
  Ext.apply(Ext.ux.grid.filter.BoolFilter.prototype, {
    yesText: _('Yes'),
    noText: _('No')
  });

if(Ext.ux.grid.filter.StringFilter)
  Ext.apply(Ext.ux.grid.filter.StringFilter.prototype, {
    emptyText: _('Enter Filter Text...')
  });

if(Ext.ux.grid.filter.NumericFilter) {
  Ext.apply(Ext.ux.grid.filter.NumericFilter.prototype, {
    menuItemCfgs: {
      emptyText: _('Enter Filter Text...'),
      selectOnFocus: true,
      width: 125
    }
  });
}

if(Ext.ux.grid.filter.DateFilter)
  Ext.apply(Ext.ux.grid.filter.DateFilter.prototype, {
    afterText: _('After'),
    beforeText: _('Before'),
    onText: _('On#DateFilter').split('#')[0],
    dateFormat: _('m/d/Y#DateFilter').split('#')[0]
  });
