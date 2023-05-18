/*
 Copyright by Andreas W. Pross 2023
*/
"use strict";

$.widget("sx.sxTable", {
  options: {
    columns: [
      { fieldname: "field1", caption: "Field 1" },
      { fieldname: "field2", caption: "Field 2" },
      { fieldname: "_buttons", caption: "Actions" }
    ],

    getEditorValue: function (e) {
      return undefined;
    },

    setEditorValue: function (e) {
      return false;
    },
  },

  _create: function () {
    var self = this;
    var table = self.element;
    var tbody = table.find('tbody');

    tbody.find('tr[data-template-row]').each((index, val) => {
      $(val).hide();
    })
  },

  _init: function () {

  },

  _buttonHandler: function (e) {
    if ($(this).attr("data-action")) {
      var row = $(this).closest("tr[name=row]");

      if (e.data.row) {
        switch ($(this).attr("data-action")) {
          case "editrow":
            e.data.instance._startEdit(e.data);
            break;
          case "deleterow":
            e.data.instance._delete(e.data);
            break;
          case "cancelrow":
            e.data.instance._cancelEdit(e.data);
            break;
          case "acceptrow":
            e.data.instance._endEdit(e.data);
            break;
        }
        e.preventDefault();
      }
    }
  },

  _destroy: function () {

  },

  _setOption: function (key, value) {
    this.options[key] = value;
  },

  fromJson: function (data) {
    var self = this;
    var dataObj = JSON.parse(data);

    self.addRows(dataObj);
  },

  getChangeSet: function () {
    var self = this;

    var table = self.element;
    var tableBody = table.find('tbody');

    if (self._currentEdit) {
      if (!self._endEdit(self._currentEdit)) {
        return false;
      }
    }

    var data = [];

    tableBody.children("tr").each(function (index, value) {
      var rowdata = $(value).data("datarow");
      var changed = $(value).data("changedvalues");
      var status = $(value).attr("data-status");
      if (rowdata && (changed || status)) {
        var rowresult =
          data.push({
            action: status,     // deleted, added, changed
            original: rowdata,
            changed: changed
          });
      }
    });

    return JSON.stringify(data);
    unset(data);
  },

  mergeChanges: function () {
    var self = this;
    var table = self.element;
    var tableBody = table.find('tbody');

    if (self._currentEdit) {
      if (!self._endEdit(self._currentEdit)) {
        return false;
      }
    }

    tableBody.children("tr").each(function (index, value) {
      var rowdata = $(value).data("datarow");
      var changed = $(value).data("changedvalues");
      var status = $(value).attr("data-status");
      if (rowdata && (changed || status)) {
        if (status == "deleted") {
          $(value).remove();
        } else {
          var datarow = Object.assign({}, rowdata, changed)
          $(value).data("datarow", datarow);
          $(value).attr("data-status", null);
          self._leaveEditMode({
            instance: self,
            row: value,
            datarow: datarow
          })
        }
      }
    });

    return true;
  },

  clear: function () {
    var self = this;

    self.element.find('tbody').children('tr').each(function (index, value) {
      var $value = $(value);
      var attr = $value.attr("data-template-row")
      if (typeof attr == 'undefined' || attr == false) {
        $value.remove();
      }
    })
  },

  _currentEdit: null,

  addRows(data) {
    var self = this;
    var newRows = [];

    var $newTemplate = self.element.find("tbody").find("[data-template-row='default']");

    data.forEach(function (datarow) {
      var $newRow = $newTemplate.clone();
      $newRow.data('datarow', datarow);
      $newRow.attr('data-template-row', '');
      $newRow.attr('name', 'row');
      $newRow.attr('data-row-type', 'default');
      $newRow.attr('data-status', 'unchanged');
      $newRow.find("[data-hide-empty]").hide();

      Object.entries(datarow).forEach(function (item, index) {
        var $datacell = $newRow.find("[data-fieldname='" + item[0] + "']");
        if ($datacell.length >= 1) {
          // Set Editor value
          var ret = self.options.setEditorValue({
            data: datarow,
            value: item[1],
            fieldname: item[0],
            $row: $newRow,
            rowtype: 'default',
            $datacell: $datacell
          });

          if (ret != true) {
            $datacell.each((index, value) => {
              var $cell = $(value);
              if ($cell.is("select") || $cell.is("input")) {
                $cell.val(item[1]);
              } else {
                $cell.text(item[1]);
              }
            });
          }
        }
      });

      $newRow.show();
      newRows.push($newRow);
    })

    $newTemplate.after(newRows);
  },

  addRow: function (datarow) {
    this._addMultipleData([datarow]);
  },

  _startEdit(e) {
    var self = e.instance;
    var table = self.element;
    var tableBody = table.find('tbody');

    if (self._currentEdit !== null) {
      if (!self._endEdit(e.instance._currentEdit)) {
        return false;
      }
    }

    self._currentEdit = e;

    tableBody.find("[name=btnrowedit]").hide();
    tableBody.find("[name=btnrowdelete]").hide();

    // change buttons
    var newCol = self._getBtnEditMode({ datarow: e.datarow });
    e.buttoncontainer.empty()
    e.buttoncontainer.append(newCol);

    // change editors
    var cols = self.element.find('thead tr').children("th");
    var currentColumns = e.row.children("td");

    cols.each((index, value) => {
      var fieldname = $(value).data("fieldname");
      var column = self._getColumnOptionsByName(fieldname);
      if (column !== null && column.editable == true) {
        var currentVal = e.datarow[fieldname];
        var changes = e.row.data("changedValues");
        if (changes && changes.hasOwnProperty(fieldname)) {
          currentVal = changes[fieldname];
        }

        var editor = self.options.templateEditor({
          datarow: e.datarow,
          value: currentVal,
          instance: self,
          buttoncontainer: e.buttoncontainer,
          fieldname: fieldname
        });
        editor.attr("name", "editfield");
        $(currentColumns[index]).empty();
        $(currentColumns[index]).append(editor);
      }
    });

    return true;
  },

  _leaveEditMode(e) {
    var self = e.instance;
    var table = self.element;
    var tableBody = table.find('tbody');

    tableBody.find("[name=btnrowedit]").show();
    tableBody.find("[name=btnrowdelete]").show();

    if (!e.buttoncontainer){
      e.buttoncontainer = e.row.find("[name=buttoncolumncontainer]");
    }

    // replace buttons
    var newCol
    if (e.row.attr("data-status") == "deleted") {
      newCol = self._getBtnUndoMode({ datarow: e.datarow });
    }else{
      newCol = self._getBtnNormalMode({ datarow: e.datarow });
    }
    e.buttoncontainer.empty()
    e.buttoncontainer.append(newCol);

    // change editors
    var currentColumns = e.row.children("td");
    var cols = self.element.find('thead').children("tr").children("th");
    cols.each((index, value) => {
      var fieldname = $(value).data("fieldname");
      var column = self._getColumnOptionsByName(fieldname);
      if (column !== null && column.editable == true && column.fieldname !== "_buttons") {
        var currentVal = e.datarow[fieldname];
        var changes = e.row.data("changedvalues");
        if (changes && changes.hasOwnProperty(fieldname)) {
          currentVal = changes[fieldname];
        }

        var newDataCol = self.options.templateDataContent({
          datarow: e.datarow,
          value: currentVal,
          instance: self,
          buttoncontainer: e.buttoncontainer,
          fieldname: fieldname
        });
        $(currentColumns[index]).empty();
        $(currentColumns[index]).append($(newDataCol));
      }
    });

    self._currentEdit = null;

    return true;
  },

  _cancelEdit(e) {
    var self = e.instance;
    e.row.attr("data-status", "");
    e.row.data("changedvalues", null);
    self._leaveEditMode(e);
    return true;
  },

  _endEdit(e) {
    var self = e.instance;
    var table = self.element;
    var tableBody = table.find('tbody');
    var changedValues = {};

    // change editors
    var currentColumns = e.row.children("td");
    var cols = self.element.find('thead').children("tr").children("th");
    cols.each((index, value) => {
      var fieldname = $(value).data("fieldname");
      var column = self._getColumnOptionsByName(fieldname);
      if (column !== null && column.editable == true && column.fieldname !== "_buttons") {

        var newVal = self.options.editorGetValue({
          fieldname: fieldname,
          editor: $(currentColumns[index]).children("[name=editfield]")
        });

        changedValues[fieldname] = newVal;
      }
    });

    // save data....
    e.row.data("changedvalues", changedValues);
    if (e.row.attr("data-status") !== "added"){
      e.row.attr("data-status", "changed");
    }

    self._leaveEditMode(e);
    return true;
  },

  _delete(e) {
    var self = e.instance;

    e.row.data("changedValues", null);
    e.row.attr("data-status", "deleted");
    self._leaveEditMode(e);
    return true;
  },

  _getColumnOptionsByName(fieldname) {
    var ret = null;

    this.options.columns.forEach((value) => {
      if (value.fieldname == fieldname) {
        ret = value;
      }
    });

    return ret;
  }
});
