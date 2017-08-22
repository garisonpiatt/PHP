<?
/* ************************************************************************ */
/*                           MySQL Database Class                           */
/* ************************************************************************ */
/*
**  Database Abstraction class for MySQL
**  v 1.0.1
**
**  by: Garison Piatt
**      garison@garisonpiatt.com
**      http://garisonpiatt.com
**
**  Created:      08/10/07
**  Last Update:  01/18/09
**
**  Description:
**    This class isolates the parent application from the inner workings of the
**    MySQL database.  Invoking this class allows the application to search and
**    modify a given database without the burden of forming select, insert, and
**    update queries.
**
**  Public Functions:
**    Following is a list of public functions available, with allowable parameter
**    types.  Full details of each function can be found in the Public section.
**      object mysql(string db)
**      array structure()
**      string error(string type)
**      bool rewind()
**      bool seek(integer pos)
**      string sortBy(string order)
**      string getTableComment(void)
**      char setPrefix(char letter)
**      char setFormat(char format)
**      resource saveResource(void)
**      void restoreResource(resource res)
**      mixed query(string query)
**      mixed find(array search, bool similar [opt], array link [opt])
**      mixed select(string fields, array search, bool similar [opt], array link [opt])
**      mixed insert(array data)
**      mixed update(array data, array search, bool similar [opt], integer limit [opt])
**      array fetch()
**      string fetchField(string field)
**      mixed delete( array search, bool similar [opt], integer limit [opt])
*/


define('ID_FLD', 'id');

// We can't do anything with any database unless we're connected to it.  This
// function handles the database initial connection.  It can take a server name,
// login name and password as parameters, but, if these are not supplied, it
// will use the default definitions for those values (this is to maintain
// backward compatibility with older sites).
// If successful, this function will return a link to the database resource;
// otherwise, it sends an error to the administrator, and shows a fatal message
// on the visitor's browser.
  function connectToDB($dbServer=null,$dbName=null,$dbLogin=null,$dbPassword=null)  {
    if (empty($dbServer))     $dbServer   = DB_SERVER;
    if (empty($dbLogin))      $dbLogin    = DB_LOGIN;
    if (empty($dbPassword))   $dbPassword = DB_PASSWORD;
    if (empty($dbName))       $dbName     = DB_NAME;
    $link = @mysqli_connect($dbServer, $dbLogin, $dbPassword);
    if ($link && mysqli_select_db($link, $dbName)) return($link);


    $err = (isset($GLOBALS['php_errormsg']) ? $GLOBALS['php_errormsg'] :
            ($err=@mysqli_error($link) ? $err : 'unknown'));

    echo
      "<html><body>\n".
      "  <blockquote style=\"width:350px;padding:5px;border:2px solid Orange;background-color:Yellow;\">\n".
      "    An error has occurred connecting to the database\n".
      "    which may make it impossible to complete this\n".
      "    operation.  The webmaster has already been notified\n".
      "    of this problem.  We apologize for any inconvenience.\n".
      "  </blockquote>\n".
      "</body></html >\n";
    echo "<!--\n".
         "Host: {$_SERVER['HTTP_HOST']}\n".
         "Server/Login: $dbServer/$dbLogin\n".
         "Database: $dbName\n".
         "URI: {$_SERVER['REQUEST_URI']}\n".
         "Reason: $err\n".
         "-->\n";

    exit();
  }

 // Connect to the database.
  $GLOBALS['dbConnectionLink'] = connectToDb();



// All of the database access functions are contained in this class.  Each instance
// of this class will handle all operations for one database table.
// To facilitate working with multiple and/or normalized database tables, several of
// the functions below will include a _join version, which creates queries that work
// across various tables.  Eventually.
class mysql {

  // These variables are used to maintain the status of the database class.  They are
  // not accessible globally, and must be accessed via the set and get functions.
  var $dbLinkID=null; // resource link identifier for current database
  var $db;            // current database table name
  var $join;          // list of joins to add to FROM clause
  var $dbret;         // db resource returned from @mysql_query calls.
  var $rowcnt;        // number of rows affected/returned from previous query
  var $errormsg;      // database error message (from mysql_error() call)
  var $errornum;      // database error number (from mysql_errno() call)
  var $sortOrder;     // sort-by field list, with optional direction indicators
  var $isMultiple;    // True if multiple tables are being used.
  var $lastQuery;     // Full text of last query executed
  var $saveResource;  // Save location for DB resource; used for temp overrides


/* ************************************************************************ */
/*                             mysql Constructor                            */
/* ************************************************************************ */
  /** mysql contructor
  ** This function initializes the object with the name of the database table given
  ** in the object creation.  This instance of the object will only use that table.
  **
  ** This function may be expanded to include multiple table, for the use in joins.
  **
  **  Inputs:
  **    db        name of database table to use for all functions
  **    link      optional database resource link
  **
  **  Returns:
  **    nothing (resource pointer is created externally)
  */
  function mysql($db,$link=null) {
   // First, determine if we are connected to the database.  There's no "if exists"
   // query in MySQL, so we have to assume that the database is connected if the
   // link to it is defined.  The whole process dies if we can't proceed.
    $this->dbLinkID = (!empty($link) ? $link : $GLOBALS['dbConnectionLink']);
    if (!$this->check_connected())    exit($this->error());

   // We want to be able to define multiple databases in an array, with the keys
   // being the table designator.  However, the DB name has to be in the previous
   // "db as x" format, in order to avoid major changes to this code.  So, we have
   // to take the array, and convert it to the string we're trying to avoid.
    $this->join = null;       // make sure there are no joins to start.
    if (is_array($db)) {
     // The database definition is an array, so we have to turn it into a string.
      $str_db = $conj = '';
      foreach ($db as $k=>$v) {
       // If the key is "left-join", then we need to process the list of joins.
       // the joins will be in an array (and this will only happen once), with
       // a format similar to that of a regular DB definition, except that an
       // "ON" clause is included.
        if ($k=='left-join') {
         // We're too lazy to figure out the proper array structure for joins, so
         // we'll just assume that the strings are properly formatted.
          foreach ($v as $jk=>$kv)  $this->join .= "left join $kv ";
          continue;
        }

       // Add each database name to the string.  If a (non-numeric) key is
       // available, turn that into the designator.
        $str_db .= $conj.$v.(is_numeric($k) ? '' : ' as '.$k);
        $conj = ',';
      }
      $db = $str_db;        // overwrite the original definition
    }

   // We are connected, so now we check to see if the database table name (or
   // names) provided is valid.  (It is, if we can perform an operation on it.)
   // We allow here for the possibility of multiple databases, with optional
   // name assignments.  This means we have to break up the list of tables, and
   // reduce each to only the name.
    $dbcnt = 0;           // We need to count the number of tables, for several reasons
    $dblist = explode(',', "$db,");     // the comma assures the result will be an array
    foreach ($dblist as $tdb) {
     // Skip this table if it's not defined.
      if (empty($tdb))    continue;
     // If we can't read anything from the table, that means it doesn't exist.  And
     // that, of course, is a fatal error.  To test the table, we have to strip off
     // any name assignment strings.
      $tdbrow = explode(' ', $tdb);          // Strip down to table name only
      $tdb = $tdbrow[0];
      if (!@mysqli_query($this->dbLinkID, "show columns from $tdb")) {
        $this->force_error("Database table '$tdb' does not exist");
        exit($this->error());
      }
      $dbcnt++;           // Increment the number of working database tables
    }
   // Someone might try to play a trick on us, and give us a list of blank table names.
   // In that event, the table counter will still be zero.
    if ($dbcnt==0) {
      $this->force_error("No database name provided");
      exit($this->error());
    }

   // Otherwise, save the name of the database table.
    $this->db = $db;                          // save the DB name
    $this->sortOrder  = null;                 // natural sort order
    $this->isMultiple = ($dbcnt>1);           // set the multiple db indicator
    $this->saveResource = null;               // Clear the saved resource
   // Set the default ID prefix.  Since groups of tables can all start with the same
   // letter, we do this to try to ensure that the prefixes are distinct.
    $X = explode('_', strtoupper("_$db"));      // this ensures the result is an array
    $this->idPrefix   = $X[(count($X)-1)][0];   // set the default ID prefix
   // We're done, but there's no status to return
    return;
  }



/* ************************************************************************ */
/*                             Private Functions                            */
/* ************************************************************************ */

  /** check_connected
  ** This function checks to see if a database connection is available.  It
  ** assumes that the the database connection has been tested externally, and
  ** that the database link value associated with this object will be set if
  ** the connection is made.  (This may not be the best test, but it's the only
  ** one we have so far.)  This test should be at the start of all functions.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    true if DB connected (return value can be ignored); or
  **    false if no connection
  */
  function check_connected() {
   // If connected, just return.
    if ($this->dbLinkID)    return true;

   // Otherwise, it's an error, and we have to register it.
    $this->force_error('No connection found to database');
    return false;
  }



  /** buildFromClause
  ** This function constructs the query's FROM clause by piecing together the
  ** table names and join clauses.  The table name should always exist.  If the
  ** join clause is not available, it is ignored.
  **
  **  Inputs:
  **    none
  */
  function buildFromClause() {
   // The FROM clause starts with the database table name(s).  We surround this
   // string with parentheses in case there is a join clause: a bug in MySQL 5+
   // makes it think comma-joined table are in a different scope, so it "forgets"
   // those table names.  The parentheses somehow fix this.
    $from = '('.$this->db.')'.
        (!empty($this->join) ? ' '.$this->join : null);
    return $from;
  }


  /** buildAssignmentList
  ** This function creates the list of assignment values for insert and update operations.
  **
  **  Inputs:
  **    data      an array of data to write into the record.
  **              The format of this array is field/value, as with the find function.
  */
  function buildAssignmentList($data) {
   // Scan through the data list, and create the assignment list.  Since the record
   // ID is set to auto-increment, we don't need to define it, for either inserts or
   // updates (we hope).  If the ID field is defined in the list, eliminate it.
    $set = $conj = '';
    foreach($data as $fld=>$val) {
     // If ID field is already in the list, skip over the definition.
      if ($fld==ID_FLD)   continue;
     // Add the field to the set list.  This assumes that all values are numbers and/or
     // strings.  We'll have to add support for other types later.
      $set .= $conj . "$fld=\"".mysqli_real_escape_string($this->dbLinkID,$val)."\"";
      $conj = ', ';
    }
   // An obvious error if there's nothing to save.
    if (empty($set)) {
      $this->force_error("No data to store in database '{$this->db}'");
      return false;
    }
    return $set;
  }


  /** buildWhereClause
  ** This function scans the field list, and builds a where clause from the contents.
  **
  **  Inputs:
  **    search    defines the search criteria, using the same field/value array format.
  **              Alternatively, this can be a string containing a preformatted search
  **              clause.  This string is not checked for validity.
  **    similar   an optional parameter indicating whether the match on all fields is
  **              exact or similar (the default is “exact”).
  **
  **  Returns:
  **    complete where clause, or blank, if no search data provided.
  */
  function buildWhereClause($search, $similar) {
   // Just exit if no search data is available.
    if (empty($search))     return '';

   // If the search parameter is not an array, then we assume it to be a preformatted
   // search string, and return it directly.
    if (!is_array($search))     return " where $search ";

   // Scan the field list, and build the where clause from the entries.  There is
   // still a chance that the entries are not defined, so we check for that as well.
    $where = $conj = '';
    while (list($fld,$val) = each($search)) {
     // If the field name is a number, it means this portion of the where clause
     // is being presented as a complete string in the value.
      if (!is_numeric($fld))  $val = mysqli_real_escape_string($this->dbLinkID, $val);
      $where .= $conj . (is_numeric($fld) ? $val :
                          ($similar? "$fld like \"%$val%\"" : "$fld=\"$val\""));
      $conj = ' && ';
    }
    if (!empty($where))   $where = " where $where ";
    return $where;
  }


  /** addLinkList
  ** If a link list is defined, concatenate the items in the list into an extension
  ** for the where clause.  All pairs in the list must be equivalent, not similar.
  ** This function assumes that it is not called unless the link list is available.
  **
  **  Inputs:
  **    where     current where clause
  **    links     list of links to add to where clause
  **              the links are assumed to be field names only, and exact matches
  **
  **  Returns:
  **    complete where clause
  */
  function addLinkList($where, $link) {
    if (!empty($link)) {
     // The starting conjunction depends on whether or not there is a where clause.
      $conj = (empty($where) ? '' : ' && ');
     // The items are treated only as equivalents, and are assumed to be field names.
      foreach ($link as $t1=>$t2) {
        $where .= "$conj$t1=$t2";           // add this link to the where clause
        $conj = ' && ';                     // the conjunction is always "and"
      }
    }
    return $where;
  }


  /** getLimitClause
  ** Analyze the limit information to create a limit clause for the select of
  ** find operation.  The value can be an integer, meaning it's the number
  ** of rows to return, or a string or array containing the starting record
  ** and row count.  Note that no error checking is performed here.  It is the
  ** responsibility of the programmer to do things correctly.
  **
  **  Inputs:
  **    limit     limit information (integer, string, or array)
  **
  **  Returns:
  **    complete limit clause
  */
  function getLimitClause($limit) {
    $limitClause = '';
    if (!empty($limit)) {
     // For integer or string values, just tack the numbers onto the end of
     // the limit tag.  Arrays have to be converted to a formatted string.
      $limitClause = ' limit '.
          (is_numeric($limit) || is_string($limit) ? $limit : join(',',$limit));
    }
    return $limitClause;
  }


  /** register_error
  ** This function records the MySQL error from the previous query.  It only saves
  ** the message; it does not act upon it in any way.  The error messages are for
  ** reference via the error() function.
  **
  **  Inputs:
  **    prefix    optional string to prepend to text error message
  **
  **  Returns:
  **    nothing
  */
  function register_error($prefix='') {
   // Register the MySQL error values.
    $this->errormsg = (!empty($prefix) ? "$prefix: " : '') .
                        mysqli_error($this->dbLinkID);
    $this->errornum = mysqli_errno($this->dbLinkID);
    return;
  }


  /** force_error
  ** This function forces a pseudo-MySQL error when there is no equivalent error.
  ** Such cases may be: no connection; invalid parameters, no returned data, etc.
  **
  **  Inputs:
  **    errmsg    text error message
  **
  **  Returns:
  **    nothing
  */
  function force_error($errmsg='') {
   // Register the MySQL error values.
    $this->errormsg = $errmsg;
    $this->errornum = -1;
    return;
  }


/* ************************************************************************ */
/*                             Public Functions                             */
/* ************************************************************************ */

  /** structure
  ** This function returns the structure of the database table in an array.
  ** If a field name is specified, only the structure information for that
  ** field will be returned.
  **
  **  Inputs:
  **    onlyfld       - optional field name
  **
  **  Returns:
  **    false, if structure is not available; or
  **    an array containing the structure of the database table, in expanded detail.
  **    Each row in the array describes one field; all fields are returned in the order
  **    in which they appear in the table.  The columns in each row are as listed below:
  **     1. the field name, as it appears in the table;
  **     2. the field type, such as string, enum, date, etc.;
  **     3. the size of the field (an integer, or blank if unused);
  **     4. for enum and set only, the possible field values;
  **     5. a flag set to true if the field can contain null values;
  **     6. the key type (primary, unique, multi-index) of the field, if any;
  **     7. the default value for the column; and
  **     8. extra information, such as auto-increment status.
  */
  function structure($onlyfld=null) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // The use of multiple tables is permitted, but some operations (such as this one)
   // will not work with them.  So exit if multiple tables are being used.
    if ($this->isMultiple) {
      $this->force_error("Cannot provide structure information for multiple tables");
      return false;
    }

   // Request the column details from the database; exit if nothing found.
    $cnt = $this->query("show columns from {$this->db}");
    if (!$cnt) {
      $this->force_error("Structure not found for table {$this->db}");
      return false;
    }

   // Create the array of structure values.  Read each row, one at a time, and parse
   // the information to match the output structure.
    $struc = array();
    while ($cnt--) {
     // Extract the column description from each row.  Break the row information down,
     // and store it in the appropriate variables.
      $row = $this->fetch(MYSQL_BOTH);
      if ($row===false)     break;
     // Get the field name, and store it as it appears.  If a specific field was
     // provided in the function call, and this field does not match that one,
     // just ignore the entire row.
      $fld = $row[0];
      if (!empty($onlyfld) && $fld != $onlyfld)   continue;

     // Extract the field type and length.  If no length (text, date, etc), the $len
     // field will be blank.
      list($typ,$len) = explode('(', str_replace(')','',$row[1]));
     // For enum and set fields, the value list may be corrupted if there is a paren
     // embedded in any of the values.  Therefore, parse the original string again.
      if ($typ=='enum' || $typ=='set') {
       // Store the list values separately in an array.  We just strip off the type
       // value and parens, then break apart the individual itesm at the commas.
        $lst = explode("','", str_replace("$typ('",'',substr($row[1],0,-2)));
       // Now scan the array, and record the length of the longest value.  Also, the
       // value may contain escaped slashes, which will cause problems in other places,
       // so strip them out now.  The length saved is the unescaped length.
        $len = 0;
        for ($x=0; $x<count($lst); $x++) {
          $lst[$x] = stripslashes(str_replace("''","'",$lst[$x]));
          if ($len<strlen($lst[$x]))   $len = strlen($lst[$x]);
        }
      } else $lst = '';               // If not enum or set, $lst is blank
     // Set the flag for whther or not the field can be NULL.
      $nul = ($row[2]=='YES' ? true : false);
     // If a key type (PRI, UNI, MUL) is defined, save the expanded version.
      $key = ($row[3]=='PRI'?'primary' : ($row[3]=='UNI'?'unique' : ($row[3]=='MUL'?'index' : '')));
     // Save the default value and extra attributes fields
      $def = ($row[4]=='NULL'||$row[4]==null ? null : $row[4]);
      $ext = $row[5];

     // Now, add all of that information to the structure array.
      $struc[] = array($fld, $typ, $len, $lst, $nul, $key, $def, $ext);
    }

   // Return the array.  The caller will deal with what the contents mean.
    return $struc;
  }


  /** error
  ** This function returns an message reporting the error status of the previous
  ** database operation.  If no error occurred, this function will return false.
  ** An optional parameter will return an error number instead of a text string,
  ** or a combination of the two.
  **
  **  Inputs:
  **    type      empty for text message; 'num' for number only; 'both' for combination
  **
  **  Returns:
  **    string containing error message
  */
  function error($type='text') {
   // If the error type is 'num', just return the error number previously collected.
    if ($type=='num')     return $this->errornum;
   // Otherwise, return the text version of the error message.  If the error type is
   // 'both', append the error number, in parentheses.
    return $this->errormsg . ($type=='both' ? " ({$this->errornum})" : '');
  }


  /** escape
  ** Escape a string for insertion in the database table.  We treat this as an
  ** embedded function because the MySQl-I version of the function requires the
  ** database link ID as a parameter.
  **
  **  Inputs:
  **    str       string to escape
  **
  **  Returns:
  **    escaped string
  */
  function escape($str) {
    return mysqli_real_escape_string($this->dbLinkID, $str);
  }


  /** rewind
  ** This function rewinds the table pointer to the first record corresponding to
  ** the previous search.  It is identical to seek(0).
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    true, if successful; or
  **    false if an error occurred.
  */
  function rewind() {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;
    
    return $this->seek(0);
  }


  /** seek
  ** This function will advance or rewind the table data pointer to a selected position.
  ** If the position exceeds the number of records in the result set, the current pointer
  ** will not be changed.  The result number provided is assumed to be relative to the
  ** start of the data set.  Note that this function operates only on the returned data
  ** set from the previous query, not the entire database table.
  **
  **  Inputs:
  **    pos       an integer representing the position number.
  **
  **  Returns:
  **    true, if successful; or
  **    false, if error, or if position exceeds number of rows
  */
  function seek($pos) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // Move the result pointer, and exit if successful.  We do it this way, instead of
   // testing against our internal values, so the MySQL error can be registers.
    if (mysqli_data_seek($this->dbret,$pos))    return true;
   // If there was an error, register it, and return false to the caller.
    $this->register_error("Cannot seek to position $pos");
    return false;
  }


  /** sortBy
  ** Set the order-by criteria.  For subsequent queries (specifically find and
  ** search), this value will be used to form the "order by" clause.
  **
  **  Inputs:
  **    order     value to use for next query
  **              Use valid field names, separated by commas.  an optional
  **              directional parameter (desc, asc) can be added to each field.
  **
  **  Returns:
  **    previous sort-by value
  */
  function sortBy($order) {
    $prev = $this->sortOrder;
    $this->sortOrder = $order;
    return $prev;
  }


  /** getTableComment
  ** MySQL allows for appending a single-line comment string to the end of the
  ** table CREATE structure.  This may be used as a comment, of course, but it
  ** may also be used to hold configuration data or operational parameters.
  ** This function returns the string to the caller, to make of it what he will.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    table comment string
  */
  function getTableComment() {
   // Get the table create structure, and strip off everything but the comment.
    $this->query('show create table {this}');
    $table = $this->fetchField('Create Table');
    list($junk,$comment) = explode('COMMENT', $table);
    return substr($comment,2,-1);   // strip equal sign and quotes before returning
  }


  /** setPrefix
  ** Set the ID code prefix.  By default, this is the first letter of the table name,
  ** but in some cases, this will cause confusion when the table names start with the
  ** same letter.
  **
  ** The prefix value is generally limited to one character.  If a string is provided,
  ** only the first letter will be used.  This can be overridden by providing a count
  ** of letters to include.  We do it this way so the programmer has to think about
  ** how many characters to include.  This prevents accidentally setting the prefix
  ** to a string which is too long for the allotted space.
  **
  **  Inputs:
  **    letter    value to use for subsequent ID codes
  **
  **  Returns:
  **    previous prefix value
  */
  function setPrefix($letter, $count=1) {
    $prev = $this->idPrefix;
    $this->idPrefix = strtoupper(substr($letter,0,$count));
    return $prev;
  }


  /** setFormat
  ** Set the ID code Format.  By default, this is the a string consisting, in order,
  ** of abbreviated year, julian date, prefix, and sequence number.  The format can
  ** be changed to allow for special circumstances, such as sequences numbers which
  ** exceed the range of the default 4 digits.  In most cases, though, the default
  ** format will suffice.
  **
  **  Inputs:
  **    format    the new format sequence, allowign only the following:
  **                  p       prefix code (1 char)
  **                  d       julian date (3 chars)
  **                  y       mod-26 encoded year ([0-9][A-Z])
  **                  r       mod-26 encoded year, reversed ([A-Z][0-9])
  **                  1-9     sequence number; length is specified by digit
  **                  -.,/_   additional characters
  **
  **  Returns:
  **    previous format value; or
  **    false, if error
  */
  function setFormat($format) {
    $prev = $this->idFormat;
    $fmt = strtolower($format);
    if (preg_match('/[^pdyr1-9\-.,\/_]/',$fmt))   return false;
    $this->idFormat = $fmt;
    return $prev;
  }


  /** saveResource, restoreResource
  ** This pair of functions is used to save and restore the DB resource in the case of
  ** a transient operation such as insert, update, or delete.  These are provided as
  ** public functions so a developer may also use them to save the DB resource when
  ** doing operations such as an alternate search.
  **
  ** Note that these functions only possess one level of iteration: they can not be
  ** used recursively to save multiple resources.  If used this way, the first 
  ** resource will be lost.
  **
  **  Inputs:
  **    saveResource:       none
  **    restoreResource:    res         optional external resource
  **
  **  Returns:
  **    saveResource:       saved resrouces value
  **    restoreResource:    false, if error; otherwise true
  */
  function saveResource() {
   // Just save the resource value and exit.
    $this->saveResource = $this->dbret;
    return $this->saveResource;
  }
  function restoreResource($res=null) {
   // If an external resource is defined, we are probably executing a recursive loop
   // somewhere, and the resource is being saved externally.  In that event, we need
   // to override the saved resource with the one provided.
    if (!empty($res))       $this->saveResource = $res;
   // If no previous resource was set, this is an error, but we don't have to report it.
    if (empty($this->saveResource))   return false;
   // Otherwise, just restore the old resource value.
    $this->dbret = $this->saveResource;
    return true;
  }


  /** query
  ** This function executes a straight, preformatted query.
  **
  **  Inputs:
  **    qry       preformated query.
  **              The current database is referenced in the query by the string “{this}”.
  **  Returns:
  **    false; or the number of records found.
  */
  function query($qry) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // Replace {this} in the query with the current table name.
    $myqry = str_replace('{this}', $this->buildFromClause(), $qry);

   // Returning the number of row affected/returned by the query depends on what that
   // query was ("select" uses a different count than other operations).  Separate
   // out the query type here, so we can use it later.
    list($type,$junk) = explode(' ',strtolower($qry));

   // Execute the query, and analyze the returned status.
    $this->dbret = @mysqli_query($this->dbLinkID, $myqry);
    $this->lastQuery = $myqry;
    if (!$this->dbret) {
     // If the query return is zero or false, it failed, so register the error.
      $this->register_error("Error performing $type operation on database '{$this->db}'\n".
                            "Query: $myqry\n");

    } else if (in_array($type, array('select','show'))) {
     // If the operation was a select, we return the number of rows in the result set.
      if (($this->rowcnt=mysqli_num_rows($this->dbret)) > 0)     return $this->rowcnt;
     // If that number is zero, it's probably an error.
      $this->force_error("No matching records in database '{$this->db}'");

    } else if (in_array($type, array('delete','insert','replace','update'))) {
     // For any other operation, return the number of rows affected.
      return (($rows=mysqli_affected_rows($this->dbLinkID)) > 0 ? $rows : true);

    } else {
     // we have to assume that if the operation was not one of select, delete, insert,
     // replace, or update. that there is no row count, so everything is okay.
      return true;
    }
   // If the code gets here, there was an error.
    return false;
  }


  /** find
  ** Search the database table for records matching the input parameters.  This operation
  ** will return all fields in the the matching records.
  **
  **  Inputs:
  **    search    an array of values to match (the array may be empty).
  **              The key of each array entry corresponds to a database field, and the
  **              value is the value to match in that field.
  **    limit     optional parameter indicating the maximum number of records to return.
  **
  **  Returns:
  **    the total number of rows found; or
  **    false, in the event of a database error
  */
  function find($search=null, $limit=null) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // Build the where clause for this operation.
    $where = $this->buildWhereClause($search, false);

   // If the limit parameter is defined, set the limit clause.
    if ($limit!==null)    $limit = $this->getLimitClause($limit);

   // If the sort-by criterion is set, create the order-by string.
    $orderby = (!empty($this->sortOrder) ? " order by {$this->sortOrder}" : '');

   // Execute the query and return the status or number of rows.
    return $this->query("select * from {this}$where$orderby$limit");
  }


  /** select
  ** This function performs a search of the table for records matching the given
  ** parameters.  It differs from the find function in that specific field names may
  ** be provided; find returns all fields in the record.  Unlike find, however, this
  ** function can only do an exact-match search.
  **
  **  Inputs:
  **    fields    a list of fields to return, comma separated;
  **    search    an array of search fields, defined as in the find function; and
  **    limit     optional parameter indicating the maximum number of records to return.
  **
  **  Returns:
  **    the total number of rows found; or
  **    false, in the event of a database error
  */
  function select($fields, $search, $limit=null) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // Build the where clause for this operation.
    $where = $this->buildWhereClause($search, false);

   // If the limit parameter is defined, set the limit clause.
    if ($limit!==null)    $limit = $this->getLimitClause($limit);

   // If the sort-by criterion is set, create the order-by string.
    $orderby = (!empty($this->sortOrder) ? " order by {$this->sortOrder}" : '');

   // IF the field list is blank, set it to all fields; it's better than forcing an error.
    if (empty($fields))   $fields = '*';

   // Execute the query and return the status or number of rows.
    return $this->query("select {$fields} from {this}$where$orderby$limit");
  }


  /** insert
  ** This function inserts a new record into the database table.  An ID code is
  ** automatically generated for this record; this assumes that there is a field
  ** in the table for the ID code.
  **
  **  Inputs:
  **    data      an array of data to write into the record.
  **              The format of this array is field/value, as with the find function.
  **
  **  Returns:
  **    false, if unsuccessful; or
  **    an ID code, automatically generated for this record
  */
  function insert($data) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // The use of multiple tables is permitted, but some operations (such as this one)
   // will not work with them.  So exit if multiple tables are being used.
    if ($this->isMultiple) {
      $this->force_error("Cannot perform insert operation with multiple tables");
      return false;
    }
   // Likewise, queries with joins defined cannot handle this operation.
    if (!empty($this->join)) {
      $this->force_error("Cannot perform insert operation with joined tables");
      return false;
    }

   // Create the assignment list.  If this returns false, it means that there were
   // no values in the source array, and the list could not be created.
    if (($set=$this->buildAssignmentList($data))===false)    return false;

   // Insert the record, and exit with the return status.
    $this->saveResource();
    $status = ($this->query("insert into {$this->db} set $set") ?
                  mysqli_insert_id($this->dbLinkID) : false);
    $this->restoreResource();
    return $status;
  }


  /** update
  ** This function performs an operation identical to insert, save that it updates an
  ** existing record, instead of creating a new one.  Fields which are not included
  ** in the data list will not be modified.
  **
  **  Inputs:
  **    data      an array of data to write into the record.
  **              The format of this array is field/value, as with the find function.
  **    search    defines the search criteria, using the same field/value array format.
  **    similar   an optional parameter indicating whether the match on all fields is
  **              exact or similar (the default is “exact”).
  **    limit     an optional limit to the number of matching entries to update.
  **              If this is used, "similar" must be defined as well.
  **
  **  Returns:
  **    false, if an error occurred; or
  **    the number of rows affected
  */
  function update($data, $search, $similar=false, $limit=1) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // The use of multiple tables is permitted, but some operations (such as this one)
   // will not work with them.  So exit if multiple tables are being used.
    if ($this->isMultiple) {
      $this->force_error("Cannot perform update operation with multiple tables");
      return false;
    }
   // Likewise, queries with joins defined cannot handle this operation.
    if (!empty($this->join)) {
      $this->force_error("Cannot perform update operation with joined tables");
      return false;
    }

   // Create the assignment list.  If this returns false, it means that there were
   // no values in the source array, and the list could not be created.
    if (($set=$this->buildAssignmentList($data))===false)    return false;

   // Build the where clause for this operation.
    $where = $this->buildWhereClause($search, $similar);

   // The limit is preset, but it can be changed.  If the limit is 'all', disregard
   // the preset value.
    $lim = ($limit=='all' ? '' : " limit $limit");

   // Update the record, and exit with the return status.
    $this->saveResource();
    $status = $this->query("update {$this->db} set $set$where$lim");
    $this->restoreResource();
    return $status;
  }


  /** fetch
  ** This function returns one record from the database table.  The contents of this
  ** record depend on the parameters of the preceding find, select, or query function.
  ** Since this is called after a query, we can assume that a DB connection exists.
  **
  **  Inputs:
  **    type        return results type: MYSQL_ASSOC (def), MYSQL_NUM, MYSQL_BOTH
  **
  **  Returns:
  **    an associative array, with the key names of each value corresponding to
  **    the table field names, or to the field name assignments in the query; or
  **    false, if there are no more entries to fetch.
  */
  function fetch($type=MYSQLI_ASSOC) {
   // Just return the results row, or false, if none exists.
    return mysqli_fetch_array($this->dbret, $type);
  }


  /** fetchObject
  ** This function returns one record from the database table, as an object.  The
  ** contents of this record depend on the parameters of the preceding find, select,
  ** or query function.
  **
  **  Inputs:
  **    none
  **
  **  Returns:
  **    an object, with the variable names of each value corresponding to the table
  **    field names, or to the field name assignments in the query;
  **    NULL, if there are no more entries to fetch.
  */
  function fetchObject() {
   // Just return the results row, or null, if none exists.
    return mysqli_fetch_object($this->dbret);
  }


  /** fetchField
  ** This function returns a specific field from a database table record.  The contents
  ** of this field depend on the parameters of the preceding find, select, or query.
  **
  **  Inputs:
  **    field     the specific field to return
  **
  **  Returns:
  **    a string containing the contents of the specified field in the current record; or
  **    false, if there are no more entries to fetch.
  */
  function fetchField($field) {
   // Get the current row from the table.  Exit if none available.
    if (($row=$this->fetch())===false) return false;
   // Otherwise, return with the contents of the specified field.
    return $row[$field];
  }


  /** getRecord
  ** This is an extension of the fetch function.  It performs the entire operation,
  ** from the find through the fetch.
  **
  **  Inputs:
  **    search    an array of values to match (the array may be empty).
  **    rec       optional record number to return
  **
  **  Returns
  **    array containing data from selected record; or
  **    false if error
  */
  function getRecord($search, $rec=null) {
    $res = $this->find($search,($rec!==null?"$rec,1":null));
    return ($res ? $this->fetch() : false);
  }


  /** getField
  ** This is an extension of the fetchField function.  It performs the entire operation,
  ** from the find through the fetch.
  **
  **  Inputs:
  **    field     name of field to return
  **    search    an array of values to match (the array may be empty).
  **    rec       optional record number to return
  **
  **  Returns
  **    field data from selected record; or
  **    false if error
  */
  function getField($field, $search, $rec=null) {
    $res = $this->select($field, $search,($rec!==null?"$rec,1":null));
    if (strpos($field,' as ')!==false)    $field = end(explode(' ',$field));
    return ($res ? $this->fetchField($field) : false);
  }


  /** delete
  ** This function will remove one or more records from the database.
  **
  **  Inputs:
  **    search    defines the search criteria, using the same field/value array format.
  **    similar   an optional parameter indicating whether the match on all fields is
  **              exact or similar (the default is “exact”).
  **    limit     an optional limit to the number of matching entries to update.
  **              If this is used, "similar" must be defined as well.
  **
  **  Returns:
  **    false, if an error occurred; or
  **    the number of rows affected
  */
  function delete($search, $limit=1) {
   // Just exit if there's no DB connection
    if (!$this->check_connected())    return false;

   // The use of multiple tables is permitted, but some operations (such as this one)
   // will not work with them.  So exit if multiple tables are being used.
    if ($this->isMultiple) {
      $this->force_error("Cannot perform delete operation with multiple tables");
      return false;
    }
   // Likewise, queries with joins defined cannot handle this operation.
    if (!empty($this->join)) {
      $this->force_error("Cannot perform delete operation with joined tables");
      return false;
    }

   // Too many times, we've tried to delete a record using only the ID code.  And
   // we've struggled to figure out why it wasn't working.  So, now we reduce the
   // dependence on strict, array-based where clauses by searching for the ID code
   // if the search parameter provided is only an integer.
    if (is_integer($search)) {
     // The first item in the structure is the first field in the database, and the
     // first value in that sub-array is the field name.
      $struct = $this->structure();
      $search = array($struct[0][0]=>$search);
    }

   // Build the where clause for this operation.
    $where = $this->buildWhereClause($search, false);

   // The limit is preset, but it can be changed.  If the limit is 'all', disregard
   // the preset value.
    $lim = ($limit=='all' ? '' : " limit $limit");

   // Perform the delete, and return the status
    $this->saveResource();
    $status = $this->query("delete from {$this->db}$where$lim");
    $this->restoreResource();
    return $status;
  }

// end of class mysql
}
?>