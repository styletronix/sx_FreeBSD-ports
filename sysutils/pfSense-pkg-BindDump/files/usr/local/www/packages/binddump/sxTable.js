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
        .addClass("tn-group pull-right")
    },

    templateButtonEdit: function (e) {
      return $("<button>")
        .attr("type", "button")
        .addClass("btn btn-primary")
        .append(
          $("<span>").addClass("fa fa-edit")
        )
    },

    templateButtonDelete: function (e) {
      return $("<button>")
        .attr("type", "button")
        .addClass("btn btn-danger")
        .append(
          $("<span>").addClass("fa fa-trash")
        )
    },

    templateButtonAccept: function (e) {
      return $("<button>")
        .attr("type", "button")
        .addClass("btn btn-success")
        .append(
          $("<span>").addClass("fa fa-check")
        )
    },

    templateButtonCancel: function (e) {
      return $("<button>")
        .attr("type", "button")
        .addClass("btn btn-warning")
        .append(
          $("<span>").addClass("fa fa-undo")
        );
    },

    templateButtonColumn: function (e) {
      return $("<td>");
    },

    templateDataRowButtonColumn: function (e) {
      return $("<td>");
    },

    templateDataColumn: function (e) {
      return $("<td>").text(e?.value);
    },

    templateDataRow: function (e) {
      return $('<tr>');
    },

    templateHeadRow: function (e) {
      return this.templateDataRow(e);
    },

    templateHeadColumn: function (e) {
      return $("<th>").text(e?.column?.caption);
    },

    templateEditor: function (e) {
      return $("<input>")
        .addClass("form-control")
        .attr("type", "text")
        .value(e.value);
    },

    editorGetValue: function (e) {
      return e.editor.value();
    },

    editorSetValue: function (e) {
      e.editor.value(e.value);
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
      row.find("th").each((index, val) => {
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
    if ($(this).data("action")) {
      var row = $(this).closest("tr[name=row]");

      if (e.data.row) {
        switch ($(this).data("action")) {
          case "edit":
            e.data.instance._startEdit(e.data);
            break;
          case "delete":
            e.data.instance._delete(e.data);
            break;
          case "cancel":
            e.data.instance._cancelEdit(e.data);
            break;
          case "accept":
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

  _butRowEdit: function (but) {
    //Start the edition mode for a row.
    var $row = $(but).parents('tr');
    var $cols = $row.find('td');
    if (ModoEdicion($row)) return;

    if (!this._trigger("onBeforeBeginEdit", null, {})) {
      return;
    }

    var focused = false;
    IterarCamposEdit($cols, function ($td) {
      var cont = $td.html();
      //Save previous content in a hide <div>
      var div = '<div style="display: none;">' + cont + '</div>';
      var input = '<input class="form-control input-sm"  value="' + cont + '">';
      $td.html(div + input);  //Set new content
      //Set focus to first column
      if (!focused) {
        $td.find('input').focus();
        focused = true;
      }
    });
    SetButtonsEdit(but);

    this._trigger("onBeginEdit", null, {})
  },

  _getNewButtonColumn: function (e) {
    return _getBtnNormalMode(e);
  },

  _getBtnNormalMode: function (e) {
    var td = this.options.templateDataRowButtonColumn();
    var div = this.options.templateButtonContainer().attr("name", "btnContainer");
    div.append(this.options.templateButtonEdit().data("action", "edit"));
    div.append(this.options.templateButtonDelete().data("action", "delete"));
    td.append(div);
    return td;
  },

  _getBtnEditMode: function (e) {
    var td = this.options.templateDataRowButtonColumn();
    var div = this.options.templateButtonContainer().attr("name", "btnContainer");
    div.append(this.options.templateButtonAccept().data("action", "accept"));
    div.append(this.options.templateButtonCancel().data("action", "cancel"));
    td.append(div);
    return td;
  },

  fromJson: function (data) {
    var self = this;
    var dataObj = JSON.parse(data);

    $(dataObj).each(function (index, value) {
      self.addRow(value);
    });
  },

  clear: function () {
    var self = this;
    var table = self.element;

    table.find('tbody').empty;
  },

  addRow: function (datarow) {
    var self = this;
    var table = self.element;

    var row = table.find('thead tr');
    var cols = row.find('th');
    var tableBody = table.find('tbody');

    var newRow = self.options.templateDataRow({ datarow: datarow });
    newRow.data('datarow', datarow);
    newRow.attr('name', 'row');

    cols.each((index, value) => {
      var newCol;
      var column = self._getColumnOptionsByName($(value).data("fieldname"));
      if (column === null) {

      } else if ($(value).data("fieldname") === '_buttons') {
        newCol = self._getBtnNormalMode({ datarow: datarow });
        newCol.on("click", "button",
          {
            instance: self,
            datarow: datarow,
            row: newRow,
            buttoncontainer: newCol
          },
          self._buttonHandler
        );
      } else {
        newCol = self.options.templateDataColumn({
          datarow: datarow,
          column: column,
          value: datarow[$(value).data("fieldname")]
        });
      }
      newRow.append(newCol);
    });

    tableBody.append(newRow);
  },

  _startEdit(e) {
    // instance
    // datarow
    // row
    // buttoncontainer
    var col = e.buttoncontainer.parent();
    e.buttoncontainer.remove();

    var newCol = e.instance._getBtnEditMode({ datarow: e.datarow });
    col.append(newCol);
    newCol.on("click", "button", e, e.instance._buttonHandler);
    e.buttoncontainer = newCol;
  },

  _cancelEdit(e) {
      var col = e.buttoncontainer.parent();
      e.buttoncontainer.remove();

      var newCol = e.instance._getBtnNormalMode({ datarow: e.datarow });
      col.append(newCol);
      newCol.on("click", "button", e, e.instance._buttonHandler);
      e.buttoncontainer = newCol;
  },

  _endEdit(e) {
    e.instance._cancelEdit(e);
},

_delete(e) {
  alert("delete");
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


