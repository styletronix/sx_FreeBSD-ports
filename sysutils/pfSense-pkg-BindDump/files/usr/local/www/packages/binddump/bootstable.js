/*
Bootstable
 @description  Javascript library to make HMTL tables editable, using Bootstrap
 @version 1.1
 @autor Tito Hinostroza
*/
"use strict";
//Global variables
var params = null;  		//Parameters
var colsEdi = null;
var newColHtml = '<div class="btn-group pull-right">' +
  '<button id="bEdit" type="button" class="btn btn-primary" onclick="butRowEdit(this);">' +
  '<span class="fa fa-edit" ></span>' +
  '</button>' +
  '<button id="bElim" type="button" class="btn btn-danger" onclick="butRowDelete(this);">' +
  '<span class="fa fa-trash" ></span>' +
  '</button>' +
  '<button id="bAcep" type="button" class="btn btn-success" style="display:none;" onclick="butRowAcep(this);">' +
  '<span class="fa fa-check" ></span>' +
  '</button>' +
  '<button id="bCanc" type="button" class="btn btn-warning" style="display:none;" onclick="butRowCancel(this);">' +
  '<span class="fa fa-undo" ></span>' +
  '</button>' +
  '</div>';
// //Case NOT Bootstrap
// var newColHtml2 = '<div class="btn-group pull-right">'+
// '<button id="bEdit" type="button" class="btn btn-sm btn-default" onclick="butRowEdit(this);">' +
// '<span class="fa fa-edit" ></span>'+
// '</button>'+
// '<button id="bElim" type="button" class="btn btn-danger" onclick="butRowDelete(this);">' +
// '<span class="fa fa-trash" ></span>'+
// '</button>'+
// '<button id="bAcep" type="button" class="btn btn-success" style="display:none;" onclick="butRowAcep(this);">' + 
// '<span class="fa fa-check" ></span>'+
// '</button>'+
// '<button id="bCanc" type="button" class="btn btn-warning" style="display:none;" onclick="butRowCancel(this);">' + 
// '<span class="fa fa-undo" ></span>'+
// '</button>'+
//   '</div>';
var colEdicHtml = '<td name="buttons">' + newColHtml + '</td>';
$.fn.SetEditable = function (options) {
  var defaults = {
    columnsEd: null,         //Index to editable columns. If null all td editables. Ex.: "1,2,3,4,5"
    $addButton: null,        //Jquery object of "Add" button
    bootstrap: true,         //Indicates bootstrap is present.
    onEdit: function () { },   //Called after edition
    onBeforeDelete: function () { }, //Called before deletion
    onDelete: function () { }, //Called after deletion
    onAdd: function () { }     //Called when added a new row
  };

  params = $.extend(defaults, options);
  var $tabedi = this;   //Read reference to the current table.
  if ($tabedi.find("thead tr th[name='buttons']").length === 0){
    $tabedi.find('thead tr').append('<th name="buttons"></th>');
  }

  //Add column for buttons to all rows.
  $tabedi.find('tbody tr').each(function(index, value){
    if ($(value).find('td[name="buttons"]').length === 0){
      $(value).append(colEdicHtml);
    }
  })

  //Process "addButton" parameter
  if (params.$addButton != null) {
    params.$addButton.click(function () {
      rowAddNew($tabedi.attr("id"));
    });
  }

  //Process "columnsEd" parameter
  if (params.columnsEd != null) {
    colsEdi = params.columnsEd.split(',');
  }
};

function IterarCamposEdit($cols, action) {
  //Iterate through editable fields in a row
  var n = 0;
  $cols.each(function () {
    n++;
    if ($(this).attr('name') == 'buttons') return;  //Exclude buttons column
    if (!IsEditable(n - 1)) return;   //It's not editable
    action($(this));
  });

  function IsEditable(idx) {
    //Indicates if the passed column is set to be editable
    if (colsEdi == null) {  //no se definió
      return true;  //todas son editable
    } else {  //hay filtro de campos
      for (var i = 0; i < colsEdi.length; i++) {
        if (idx == colsEdi[i]) return true;
      }
      return false;  //no se encontró
    }
  }
}
function ModoEdicion($row) {
  if ($row.attr('id') == 'editing') {
    return true;
  } else {
    return false;
  }
}

//Set buttons state
function SetButtonsNormal(but) {
  $(but).parent().find('#bAcep').hide();
  $(but).parent().find('#bCanc').hide();
  $(but).parent().find('#bEdit').show();
  $(but).parent().find('#bElim').show();
  var $row = $(but).parents('tr');
  $row.attr('id', '');
}
function SetButtonsEdit(but) {
  $(but).parent().find('#bAcep').show();
  $(but).parent().find('#bCanc').show();
  $(but).parent().find('#bEdit').hide();
  $(but).parent().find('#bElim').hide();
  var $row = $(but).parents('tr');
  $row.attr('id', 'editing');
}

//Events functions
function butRowAcep(but) {
  var $row = $(but).parents('tr');
  var $cols = $row.find('td');
  if (!ModoEdicion($row)) return;

  IterarCamposEdit($cols, function ($td) {
    var cont = $td.find('input').val();
    $td.html(cont);
  });
  SetButtonsNormal(but);
  params.onEdit($row);
}
function butRowCancel(but) {
  var $row = $(but).parents('tr');
  var $cols = $row.find('td');
  if (!ModoEdicion($row)) return;
  IterarCamposEdit($cols, function ($td) {
    var cont = $td.find('div').html();
    $td.html(cont);
  });
  SetButtonsNormal(but);
}
function butRowEdit(but) {
  //Start the edition mode for a row.
  var $row = $(but).parents('tr');
  var $cols = $row.find('td');
  if (ModoEdicion($row)) return;

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
}
function butRowDelete(but) {
  var $row = $(but).parents('tr');
  params.onBeforeDelete($row);
  $row.remove();
  params.onDelete();
}
//Functions that can be called directly
function rowAddNew(tabId, initValues = []) {
  /* Add a new row to a editable table. 
   Parameters: 
    tabId       -> Id for the editable table.
    initValues  -> Optional. Array containing the initial value for the 
                   new row.
  */
  var $tab_en_edic = $("#" + tabId);  //Table to edit
  var $rows = $tab_en_edic.find('tbody tr');
  var $row = $tab_en_edic.find('thead tr');
  var $cols = $row.find('th');

  var htmlDat = '';
  var i = 0;
  $cols.each(function () {
    if ($(this).attr('name') == 'buttons') {
      htmlDat = htmlDat + colEdicHtml;
    } else {
      if (i < initValues.length) {
        htmlDat = htmlDat + '<td>' + initValues[i] + '</td>';
      } else {
        htmlDat = htmlDat + '<td></td>';
      }
    }
    i++;
  });
  $tab_en_edic.find('tbody').append('<tr>' + htmlDat + '</tr>');
  params.onAdd();
}
function rowAddNewAndEdit(tabId, initValues = []) {
  /* Add a new row an set edition mode */
  rowAddNew(tabId, initValues);
  var $lastRow = $('#' + tabId + ' tr:last');
  butRowEdit($lastRow.find('#bEdit'));  //Pass a button reference
}
function TableToCSV(tabId, separator) {  //Convert table to CSV
  var datFil = '';
  var tmp = '';
  var $tab_en_edic = $("#" + tabId);
  $tab_en_edic.find('tbody tr').each(function () {
    if (ModoEdicion($(this))) {
      $(this).find('#bAcep').click();
    }
    var $cols = $(this).find('td');
    datFil = '';
    $cols.each(function () {
      if ($(this).attr('name') == 'buttons') {
      } else {
        datFil = datFil + $(this).html() + separator;
      }
    });
    if (datFil != '') {
      datFil = datFil.substr(0, datFil.length - separator.length);
    }
    tmp = tmp + datFil + '\n';
  });
  return tmp;
}
function TableToJson(tabId) {   //Convert table to JSON
  var json = '{';
  var otArr = [];
  var tbl2 = $('#' + tabId + ' tr').each(function (i) {
    var x = $(this).children();
    var itArr = [];
    x.each(function () {
      itArr.push('"' + $(this).text() + '"');
    });
    otArr.push('"' + i + '": [' + itArr.join(',') + ']');
  })
  json += otArr.join(",") + '}'
  return json;
}

function TableFromJson(tabId, data) {
    var dataObj = JSON.parse(data);
    
    $(dataObj).each(function ( index, value ) {
      insertRow(tabId, value);
    });

    $("#" + tabId).SetEditable();
}

function insertRow(tabId, dataRow) {
  var tab_en_edic = $("#" + tabId);
  var row = tab_en_edic.find('thead tr');
  var keyFieldName = row.data()?.keyFieldName;
  var cols = row.find('th');

  var tableBody = tab_en_edic.find('tbody');

  var newRow = $('<tr>');
  newRow.data('dataRow', dataRow);

  cols.each(function( index, value ) {
    var newCol = $('<td>');
    var fieldName = value.dataset.fieldname;
    if (fieldName){
      newCol.text(dataRow[fieldName]);
    } else if ($(value).attr('name') == 'buttons') {
      newCol = $(colEdicHtml);
    }
    newRow.append(newCol);
  });

  tableBody.append(newRow);
}

function TableClear(tabId) {
  $("#" + tabId).find('tbody').empty();
}