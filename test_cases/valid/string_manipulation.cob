       IDENTIFICATION DIVISION.
       PROGRAM-ID. STRING-MANIP.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-FIRST-NAME       PIC X(10) VALUE "JOHN".
       01  WS-LAST-NAME        PIC X(10) VALUE "DOE".
       01  WS-FULL-NAME        PIC X(25).
       01  WS-DATE-PARTS.
           05  WS-YEAR         PIC X(4) VALUE "2023".
           05  WS-MONTH        PIC X(2) VALUE "12".
           05  WS-DAY          PIC X(2) VALUE "25".
       01  WS-FORMATTED-DATE   PIC X(10).
       01  WS-INPUT-STRING     PIC X(50) 
           VALUE "APPLE,BANANA,CHERRY,DATE".
       01  WS-DELIM-TABLE.
           05  WS-FRUIT OCCURS 4 TIMES PIC X(10).
       01  WS-COUNT            PIC 9 VALUE 0.
       01  WS-SEARCH-STR       PIC X(20) VALUE "Hello World Hello".
       01  WS-REPLACED-STR     PIC X(20).
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           DISPLAY "=== STRING CONCATENATION ===".
           INITIALIZE WS-FULL-NAME.
           STRING WS-FIRST-NAME DELIMITED BY SPACE
                  " " DELIMITED BY SIZE
                  WS-LAST-NAME DELIMITED BY SPACE
               INTO WS-FULL-NAME
               WITH POINTER WS-COUNT
               ON OVERFLOW
                   DISPLAY "STRING OVERFLOW OCCURRED"
               NOT ON OVERFLOW
                   DISPLAY "FULL NAME: '" WS-FULL-NAME "'".
           
           DISPLAY "=== DATE FORMATTING ===".
           INITIALIZE WS-FORMATTED-DATE.
           STRING WS-YEAR DELIMITED BY SIZE
                  "-" DELIMITED BY SIZE
                  WS-MONTH DELIMITED BY SIZE
                  "-" DELIMITED BY SIZE
                  WS-DAY DELIMITED BY SIZE
               INTO WS-FORMATTED-DATE
               END-STRING
           DISPLAY "FORMATTED DATE: " WS-FORMATTED-DATE.
           
           DISPLAY "=== UNSTRING OPERATION ===".
           UNSTRING WS-INPUT-STRING
               DELIMITED BY ","
               INTO WS-FRUIT(1), WS-FRUIT(2), 
                    WS-FRUIT(3), WS-FRUIT(4)
               WITH POINTER WS-COUNT
               TALLYING IN WS-COUNT
           END-UNSTRING
           DISPLAY "FRUIT 1: " WS-FRUIT(1).
           DISPLAY "FRUIT 2: " WS-FRUIT(2).
           DISPLAY "FRUIT 3: " WS-FRUIT(3).
           DISPLAY "FRUIT 4: " WS-FRUIT(4).
           DISPLAY "TOTAL ITEMS: " WS-COUNT.
           
           DISPLAY "=== INSPECT REPLACING ===".
           MOVE WS-SEARCH-STR TO WS-REPLACED-STR.
           INSPECT WS-REPLACED-STR
               REPLACING ALL "Hello" BY "Hi".
           DISPLAY "ORIGINAL: " WS-SEARCH-STR.
           DISPLAY "REPLACED: " WS-REPLACED-STR.
           
           DISPLAY "=== INSPECT TALLYING ===".
           MOVE 0 TO WS-COUNT.
           INSPECT WS-SEARCH-STR
               TALLYING WS-COUNT FOR ALL "o".
           DISPLAY "COUNT OF 'o': " WS-COUNT.
           
           STOP RUN.
