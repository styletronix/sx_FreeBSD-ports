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
    ],         //array with list of columns. [{name:"col1", caption:"Col1"},{name:"col2", caption:"Col1"}]

    templateButtonContainer: function (e) {
      return $("<div>")
        .addClass("action-icons")
    },

    templateButtonEdit: function (e) {
      var dat = $("<a>")
        .attr("title", "Edit")
        .addClass("fa fa-edit");
      dat.after("&nbsp;");
      return dat;

      // return $("<button>")
      //   .attr("type", "button")
      //   .addClass("btn btn-primary")
      //   .append(
      //     $("<span>").addClass("fa fa-edit")
      //   )
    },

    templateButtonDelete: function (e) {
      var dat = $("<a>")
        .attr("title", "Delete")
        .addClass("fa fa-trash");
      dat.after("&nbsp;");
      return dat;
      // return $("<button>")
      //   .attr("type", "button")
      //   .addClass("btn btn-danger")
      //   .append(
      //     $("<span>").addClass("fa fa-trash")
      //   )
    },

    templateButtonAccept: function (e) {
      var dat = $("<a>")
        .attr("title", "Save")
        .addClass("fa fa-check");
      dat.after("&nbsp;");
      return dat;
      // return $("<button>")
      //   .attr("type", "button")
      //   .addClass("btn btn-success")
      //   .append(
      //     $("<span>").addClass("fa fa-check")
      //   )
    },

    templateButtonCancel: function (e) {
      var dat = $("<a>")
        .attr("title", "Cancel")
        .addClass("fa fa-undo");
      dat.after("&nbsp;");
      return dat;

      // return $("<button>")
      //   .attr("type", "button")
      //   .addClass("btn btn-warning")
      //   .append(
      //     $("<span>").addClass("fa fa-undo")
      //   );
    },

    templateButtonColumn: function (e) {
      return $("<td>");
    },

    templateDataRowButtonColumn: function (e) {
      return $("<td>");
    },

    templateDataColumn: function (e) {
      return $("<td>");
    },

    templateDataContent: function (e) {
      var dat = $("<span>");
      dat.text(e?.value);
      return dat;
    },

    templateDataRow: function (e) {
      return $('<tr>');
    },

    templateHeadRow: function (e) {
      return this.templateDataRow(e);
    },

    templateHeadColumn: function (e) {
      var dat = $("<th>");
      dat.text(e?.column?.caption);
      return dat;
    },

    templateEditor: function (e) {
      var dat = $("<input>");
      dat.addClass("form-control");
      dat.attr("type", "text");
      dat.val(e.value);
      return dat;
    },

    editorGetValue: function (e) {
      return e.editor.val();
    },

    editorSetValue: function (e) {
      if (e?.editor[0]?.tagName == "SELECT"){
        dat.find('option[value="' + e.value + '"]').prop('selected', true);
      }
      e.editor.val(e.value);
    }
  },

  _create: function () {
    var self = this;
    var table = self.element;
    var thead = table.find('thead');

    var row = thead.find('tr');
    if (row.length == 0) {
      row = self.options.templateHeadRow({});
      table.append(row);
    }

    self.options.columns.forEach((value) => {
      var colFound = false;
      row.children("th").each((index, val) => {
        if ($(val).data("fieldname") == value.fieldname) {
          colFound = true;
        }
      })

      if (colFound === false) {
        var newCol = self.options.templateHeadColumn({
          datarow: null,
          column: value
        });
        newCol.data("fieldname", value.fieldname);
        row.append(newCol);
      }
    });

    // table.on("dblclick", "tbody tr", function (e) {
    //   // alert($(this).text());
    //   // e.preventDefault();
    // });


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

  _getNewButtonColumn: function (e) {
    return _getBtnNormalMode(e);
  },

  _getBtnNormalMode: function (e) {
    var div = this.options.templateButtonContainer().attr("name", "btnContainer");
    div.append(this.options.templateButtonEdit().attr("data-action", "editrow").attr("name", "btnrowedit"));
    div.append(this.options.templateButtonDelete().attr("data-action", "deleterow").attr("name", "btnrowdelete"));
    return div;
  },

  _getBtnEditMode: function (e) {
    var div = this.options.templateButtonContainer().attr("name", "btnContainer");
    div.append(this.options.templateButtonAccept().attr("data-action", "acceptrow").attr("name", "btnrowaccept"));
    div.append(this.options.templateButtonCancel().attr("data-action", "cancelrow").attr("name", "btnrowcancel"));
    return div;
  },

  _getBtnUndoMode: function (e) {
    var div = this.options.templateButtonContainer().attr("name", "btnContainer");
    div.append(this.options.templateButtonCancel().attr("data-action", "cancelrow").attr("name", "btnrowcancel"));
    return div;
  },

  fromJson: function (data) {
    var self = this;
    var dataObj = JSON.parse(data);

    $(dataObj).each(function (index, value) {
      self.addRow(value);
    });
  },

  getChangeSet: function(){
    var self = this;

    var table = self.element;
    var tableBody = table.find('tbody');

    if (self._currentEdit ){
      if (!self._endEdit(self._currentEdit)){
        return false;
      }
    }

    var data = [];

    tableBody.children("tr").each(function (index, value) {
      var rowdata = $(value).data("datarow");
      var changed = $(value).data("changedvalues");
      var status = $(value).attr("data-status");
      if (rowdata && (changed || status)){
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

  mergeChanges: function(){
    var self = this;
    var table = self.element;
    var tableBody = table.find('tbody');

    if (self._currentEdit ){
      if (!self._endEdit(self._currentEdit)){
        return false;
      }
    }

    tableBody.children("tr").each(function (index, value) {
      var rowdata = $(value).data("datarow");
      var changed = $(value).data("changedvalues");
      var status = $(value).attr("data-status");
      if (rowdata && (changed || status)){
        if (status == "deleted"){
          value.remove();
        }else{
          var datarow = Object.assign({}, rowdata, changed)
          $(value).data("datarow",  datarow);
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
    var table = self.element;

    table.find('tbody').empty();
  },

  _currentEdit: null,

  addRow: function (datarow) {
    var self = this;
    var table = self.element;

    var row = table.find('thead tr');
    var cols = row.children('th');
    var tableBody = table.find('tbody');
    var buttoncontainer;

    var newRow = self.options.templateDataRow({ datarow: datarow });
    newRow.data('datarow', datarow);
    newRow.attr('name', 'row');

    cols.each((index, value) => {
      var newCol;
      var column = self._getColumnOptionsByName($(value).data("fieldname"));
      if (column === null) {
        newCol = self.options.templateDataContent({
          datarow: datarow,
          column: column,
          value: null
        });

      } else if ($(value).data("fieldname") === '_buttons') {
        newCol = self.options.templateDataRowButtonColumn({
          datarow: datarow,
          column: column,
          value: datarow[$(value).data("fieldname")]
        });
        newCol.attr("name", "buttoncolumncontainer");
        newCol.append(self._getBtnNormalMode({ datarow: datarow }));
        newCol.on("click", "[name*='btnrow']",
          {
            instance: self,
            datarow: datarow,
            row: newRow,
            buttoncontainer: newCol
          },
          self._buttonHandler
        );
        buttoncontainer = newCol;

      } else {
        newCol = self.options.templateDataColumn({
          datarow: datarow,
          column: column,
          value: datarow[$(value).data("fieldname")]
        });
        newCol.append(self.options.templateDataContent({
          datarow: datarow,
          column: column,
          value: datarow[$(value).data("fieldname")]
        }));
      }
      newRow.append(newCol);
    });

    tableBody.append(newRow);
    newRow.attr("data-action", "editrow")
    newRow.on("dblclick", {
      instance: self,
      datarow: datarow,
      row: newRow,
      buttoncontainer: buttoncontainer
    },
      self._buttonHandler
    );
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
