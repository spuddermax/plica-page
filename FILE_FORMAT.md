PlicaPage project file format
=============================

Overview
--------
This is the specification for the PlicaPage project file format. A PlicaPage
project is a PJL file with the extension `.plica`, containing additional PJL
commands that carry the program's own state.

The format is inherited from Boomaga, whose files used the extension `.boo`.
PlicaPage **reads** Boomaga's spelling of every token below as well as its own,
so existing `.boo` projects continue to open; it **writes** only its own.


File structure
--------------
The first line of the file must be
```
<ESC>%-12345X@PJL PLICAPAGE_PROJECT
```
For backwards compatibility `@PJL BOOMAGA_PROJECT` and `@PJL BOOMAGA_PROGECT`
(Boomaga's original typo) are also accepted on read.
&lt;ESC&gt; identifies a escape control character (ASCII 27).



Project meta info
-----------------
```
@PJL PLICAPAGE META_AUTHOR="str"
```     
This command is used to specify the name of a perfomer for a document. Used when exporting to PDF.


```
@PJL PLICAPAGE META_TITLE="str"
```
This command is used to specify a title for a document. Used when exporting to PDF.


```
@PJL PLICAPAGE META_SUBJECT="str"
```  
This command is used to specify a subject for a document. Used when exporting to PDF.

  
```
@PJL PLICAPAGE META_KEYWORDS="str"
```
This command is used to specify a keywords associated with the document. Used when exporting to PDF.


Job meta info
-------------
```
@PJL PLICAPAGE JOB_TITLE="str"
```
This command is used to specify a title for a single job.



```
@PJL PLICAPAGE JOB_PAGES="pagesSpec"
```
This command is used to specify the settings for the pages in the job.
A value consists of a sequence of one page options segments separated by a comma (,).

Page spec is PageNum:Hidden:Rotation:StarBooklet
 * PageNum  -  number of the page in the source PDF. If page is a inserted blank page PageNum is letter 'B'
 * Hidden   -  if page is hidden then use letter 'H', otherwise this field is empty or omitted.
 * Rotation -  One of 0,90,180,270. If rotation is 0 this field can be omitted.
 * StarBooklet- if it's first page in booklet use letter 'S', otherwise this field is empty or omitted.
 
 
Example:
```
@PJL PLICAPAGE JOB_PAGES="1,2::180,B,3:H:90,4:H"
```
