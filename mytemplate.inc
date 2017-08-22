<?
/* ************************************************************************ */
/*                           Template Manager Class                         */
/* ************************************************************************ */
/*
**  Template Manager Abstraction class
**  v 1.0
**
**  by: Garison Piatt
**      garison@garisonpiatt.com
**      http://garisonpiatt.com
**
**  Created:      09/12/07
**  Last Update:  
**
**  Description:
**    This class manages a single template file.  The file name is saved during
**    initialization, after making sure the file exists.  The parent application
**    must assign all variables used by the template before displaying it.  An
**    embedded object of the same type may be included.
**
**    The template file must be HMTL, with limited PHP code.  The only permissible
**    PHP code is limited to echo statements ( e.g.: =$pageName) contained in PHP
**    brackets.  The template may also call the display() function of an embedded
**    object via $embeddedPage->display().  (Or a specially-named object variable;
**    see the register_object function description.)  Note that no tests are made 
**    for illegal PHP code in the template; this is merely an advisory.
**
**  Public Functions:
**    Following is a list of public functions available, with allowable parameter
**    types.  Full details of each function can be found in the Public section.
**      object  mytemplate(string file)
**      string  error()
**      void    assign(mixed var, string val [opt])
**      bool    register_object(object obj, string varName [opt])
**      void    display(void)
**      string  fetch()
*/

// We can't use global variables in the declaration area of the class, but we
// should be able to use constants.  These are some values we need.
define('TPL_DEFAULT_DIR', $_SERVER['DOCUMENT_ROOT'].'/_inc/tpl/');


// A given template may have a call to an embedded function, but the parent
// application may not have defined that object.  This will usually generate
// a run-time error.  To avoid that, we create this dummy object, to be used
// until a real template object is assigned.
class dummy {
 // This class only has a display function, and it doesn't display anything.
  function display() { /* Do nothing */ }
}


// All of the template-management functions are contained in this class.  Each 
// instance of this class will handle all operations related to displaying the
// supplied template. 
class mytemplate {

// These are global configuration fields.  They are used to change how mytemplate
// operates, either initializing or displaying the template file.  For the most
// part, these settings can be left alone.

 // These variables maintain the operating status of the template object.
  var $OK = false;                              // true if config okay; false otherwise
  var $templateFile     = '';                   // template file name
  var $templateDir      = TPL_DEFAULT_DIR;      // template file directory
  var $templateVars     = array();              // variables for template data
  var $embeddedObjects  = array();              // embedded object array
  var $parentObject     = null;                 // parent for this object
  var $errorMessage     = '';                   // error message


  /** mytemplate contructor
  ** This constructor creates the template object, and verifies that the given
  ** template file exists.  The file name is parsed for the directory name and
  ** file name; if a directory name is found, it overwrites the default name.
  ** Note that the directory path in the provided file name must be a fully-
  ** realized path name, otherwise, a basedir error may result.
  **
  **  Inputs:
  **    file      template file name
  **
  **  Returns:
  **    nothing (creates object)
  */
  function mytemplate($file) {
   // Start out by assuming that there is a problem.
    $this->OK = false;
   // Obviously, it's an error if the file name is not defined.
    if (empty($file)) {
      $this->register_error("No file name was provided");
      return;
    }
   // Parse the file name for the directory.  If one is available, overwrite the 
   // default directory name with it.
    $dir = dirname($file);
    if (!empty($dir) && $dir!='.')    $this->templateDir = "$dir/";
   // Strip the directory name from the parameter, and store the remaining file name.
    $this->templateFile = basename($file);
   // Now, make sure that the file exists.
    $thisFile = $this->templateDir.$this->templateFile;
    if (!is_file($thisFile)) {
     // Obviously, we can't do anything without the template file, so just display
     // the error and abort all operations.
      $this->register_error("File $thisFile was not found");
      echo $this->error();
      die;
    }
   // Otherwise, everything seems to be okay.  Set up a dummy embedded object, clear
   // any previous error, and set the OK flag, so we can use the other functions.
    $this->embeddedObjects['embeddedPage'] = new dummy();     // set the dummy object
    $this->clear_error();                                     // clear the error message
    $this->OK = true;                                         // permit use of all functions
    return;
  }



  /** register_error
  ** This function registers an error message for display or logging (by the parent
  ** application) at a later time.
  **
  **  Inputs:
  **    msg       The complete (pre-evealuated) error message
  **
  **  Returns:
  **    nothing
  */
  function register_error($msg) {
    $this->errorMessage = $msg;            // Save the error message.
  }

  /** error
  ** Displays the previous error message.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    String with error message, or blank
  */
  function error() {
    return $this->errorMessage;
  }

  /** clear_error
  ** Erases the previous error message.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    nothing
  */
  function clear_error() {
    unset($this->errorMessage);
    $this->errorMessage = '';
  }



  /** assign
  ** Assign a value to an internal variable.  No checks are done for illegal values.
  ** The assignments can be handled in two ways: 1) as a direct assignment pair, with
  ** the variable name in the first parameter and the value in the second; or 2) as
  ** an an array of key/value pairs, in the first parameter only.  Any type of data
  ** may be assigned, as long as it is usable by the template.
  ** Note: to avoid errors, all template values should be assigned, even if they are
  ** not used.  Assign a blank string to unneeded variables.
  **
  **  Inputs:
  **    var       variable name, or assignment array
  **    val       value to assign
  **
  **  Returns:
  **    nothing
  */
  function assign($var, $val='') {
    if ($this->OK==false)   return;         // Just exit if the file open failed

   // See if the value is an array.  If so, ignore the value parameter, and assign
   // the values from the array.
    if (is_array($var)) {
     // Scan through the array, and save the assignments
      foreach ($var as $key=>$val) {
       // ... assuming that the key is properly defined.
        if (!empty($key))    $this->templateVars[$key] = $val;
      }
    } else {
     // If the variable is not an array, just do a direct assignment.
      $this->templateVars[$var] = $val;
    }
  }



  /** register_object
  ** Register an external object for use with the current template.  The object
  ** must be of the same type (mytemplate), though only the display function will
  ** be used.  All of the current definitions will be passed to the new object,
  ** so it's a good idea to register the new object after all of the assignemnts
  ** have been made.
  ** Note that this function registers a previously-existing object; it does not
  ** create a new one.
  **
  **  Inputs:
  **    obj       new template object
  **    varName   variable name to use as reference in the template code
  **                defaults to 'embeddedPage'
  **
  **  Returns:
  **    true, if registration successful; or
  **    false, if there was an error
  */
  function register_object(&$obj,$varName=null) {
    if ($this->OK==false)   return;         // Just exit if the file open failed

   // It's an obvious error if the object is undefined.
    if (empty($obj)) {
      $this->errorMessage = "No object to register";
      return false;
    }
   // If the object is not an instance of mytemplate, we can't use it.
    if (!is_a($obj,'mytemplate')) {
      $this->errorMessage = "Registered object is not a mytemplate object";
      return false;
    }

   // We want to be able to host multiple embedded pages in one template, if need be.
   // Therefore, we need a way to reference individual pages, while not interfering
   // with existing templates.  The best way (or, at least, the method we chose) is
   // to give each embedded object a unique name, supplied in the varName parameter
   // above.  To allow existing templates to work without change, we default this to
   // 'embeddedPage' if it's not defined.  We consider it an error if multiple pages
   // share the same name.
    if (empty($varName))      $varName = 'embeddedPage';
    if ($varName!='embeddedPage' && array_key_exists($varName,$this->embeddedObjects)) {
      $this->errorMessage = "Duplicate name for object: $varName";
      return false;
    }

   // We'll get into an infinite loop if the object registers itself, so we check
   // here to make sure that the new object is not using the same file as the
   // current object, or any of its parents.
    $filename = $obj->templateDir.$obj->templateFile;     // get the new file name
    $thisObject = $this;
    do {
      if ($thisObject->templateDir.$thisObject->templateFile == $filename) {
        $this->errorMessage = "Duplicate file name in new object";
        return false;
      }
    } while ($thisObject = $thisObject->parentObject);

   // If everything is okay, save the object pointer, and pass the current variable
   // list on to the child object.  This is the simplest way of sharing variables.
    $obj->parentObject = &$this;
    $this->embeddedObjects[$varName] = &$obj;
    $this->embeddedObjects[$varName]->assign($this->templateVars);
    return true;
  }



  /** display
  ** This function displays the template file given during initialization.  Since
  ** the scope of the template file is limited to this function, the required 
  ** local and system variables must be loaded before the file.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    nothing
  */
  function display() {
    if ($this->OK==false)   return;         // Just exit if the file open failed

   // Extract the system and object variables, so they are included in the scope
   // of this function.  This is the only way the template can access them.
    extract($_SERVER);                      // get the server variables
    extract($this->templateVars);           // ... and the local variables
    extract($this->embeddedObjects);        // ... and the embedded object pointers

   // Now display the template.  This is simple: we just include the file from
   // the template directory.  It's assumed to be HTML, so it will just display.
   // Any embedded PHP code will use the variables from within this function.
    include($this->templateDir.$this->templateFile);
    return;
  }

  /** fetch
  ** This function fetches the processed contents of the template file.  Since
  ** the scope of the template file is limited to this function, the required 
  ** local and system variables must be loaded before the file.  This action is
  ** performed in the display function; the current function only sets up the
  ** data capture and return.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    false, if error, or
  **    processed template file contents
  */
  function fetch() {
    if ($this->OK==false)   return false;   // Just exit if the file open failed

   // The processed template contents are fetched by buffering the standard
   // output, processing the template file, and then extracting the buffer
   // contents.  The buffered contents are destroyed, and the processed copy
   // is returned to the calling application.
    ob_start();                           // start buffering the standard output
    $this->display();                     // "display" the template file
    $contents = ob_get_contents();        // extract the output buffer contents
    ob_end_clean();                       // destroy the buffer
   // Return the saved buffer contents to the calling application
    return $contents;
  }

// end of class mytemplate
}
?>