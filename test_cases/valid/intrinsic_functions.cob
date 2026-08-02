       IDENTIFICATION DIVISION.
       PROGRAM-ID. INTRINSIC-FUNCTIONS.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-NUMERIC-FIELD      PIC 9(5)V99 VALUE 12345.67.
       01  WS-NEGATIVE-NUM       PIC S9(5) VALUE -9876.
       01  WS-STRING-FIELD       PIC X(20) VALUE "  Hello World  ".
       01  WS-DATE-FIELD         PIC X(10).
       01  WS-TIME-FIELD         PIC X(8).
       01  WS-MAX-VAL            PIC 9(5).
       01  WS-MIN-VAL            PIC 9(5).
       01  WS-SUM-VAL            PIC 9(7).
       01  WS-AVG-VAL            PIC 9(5)V99.
       01  WS-UPPER-STR          PIC X(20).
       01  WS-LOWER-STR          PIC X(20).
       01  WS-TRIMMED-STR        PIC X(20).
       01  WS-REVERSE-STR        PIC X(20).
       01  WS-LENGTH-VAL         PIC 9(3).
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           DISPLAY "=== INTRINSIC FUNCTION TESTS ===".
           
           DISPLAY "ORIGINAL NUMBER: " WS-NUMERIC-FIELD.
           COMPUTE WS-MAX-VAL = FUNCTION MAX(100, 500, 50, 999).
           DISPLAY "MAX OF (100,500,50,999): " WS-MAX-VAL.
           
           COMPUTE WS-MIN-VAL = FUNCTION MIN(100, 500, 50, 999).
           DISPLAY "MIN OF (100,500,50,999): " WS-MIN-VAL.
           
           COMPUTE WS-SUM-VAL = FUNCTION SUM(100, 200, 300, 400).
           DISPLAY "SUM OF (100,200,300,400): " WS-SUM-VAL.
           
           DISPLAY "ABSOLUTE OF " WS-NEGATIVE-NUM " IS: " 
                   FUNCTION ABSOLUTE(WS-NEGATIVE-NUM).
           
           DISPLAY "ORIGINAL STRING: '" WS-STRING-FIELD "'".
           MOVE FUNCTION UPPER-CASE(WS-STRING-FIELD) 
               TO WS-UPPER-STR.
           DISPLAY "UPPER CASE: '" WS-UPPER-STR "'".
           
           MOVE FUNCTION LOWER-CASE(WS-STRING-FIELD) 
               TO WS-LOWER-STR.
           DISPLAY "LOWER CASE: '" WS-LOWER-STR "'".
           
           MOVE FUNCTION TRIM(WS-STRING-FIELD) 
               TO WS-TRIMMED-STR.
           DISPLAY "TRIMMED: '" WS-TRIMMED-STR "'".
           
           MOVE FUNCTION REVERSE(WS-STRING-FIELD) 
               TO WS-REVERSE-STR.
           DISPLAY "REVERSED: '" WS-REVERSE-STR "'".
           
           COMPUTE WS-LENGTH-VAL = FUNCTION LENGTH(WS-STRING-FIELD).
           DISPLAY "LENGTH OF STRING: " WS-LENGTH-VAL.
           
           MOVE FUNCTION CURRENT-DATE(1:10) TO WS-DATE-FIELD.
           DISPLAY "CURRENT DATE (YYYY-MM-DD): " WS-DATE-FIELD.
           
           STOP RUN.
