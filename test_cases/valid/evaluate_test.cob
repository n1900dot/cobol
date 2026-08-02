IDENTIFICATION DIVISION.
       PROGRAM-ID. EVALUATE-TEST.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-MENU-OPTION      PIC 9 VALUE 3.
       01  WS-EMP-TYPE         PIC X VALUE "F".
           88 FULL-TIME        VALUE "F".
           88 PART-TIME        VALUE "P".
           88 CONTRACTOR      VALUE "C".
       01  WS-GRADE            PIC 9 VALUE 2.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           DISPLAY "=== EVALUATE SINGLE VALUE ===".
           EVALUATE WS-MENU-OPTION
               WHEN 1
                   DISPLAY "OPTION 1: CREATE RECORD"
               WHEN 2
                   DISPLAY "OPTION 2: MODIFY RECORD"
               WHEN 3
                   DISPLAY "OPTION 3: DELETE RECORD"
               WHEN OTHER
                   DISPLAY "INVALID OPTION"
           END-EVALUATE.
           
           DISPLAY "=== EVALUATE WITH CONDITION ===".
           EVALUATE TRUE
               WHEN WS-EMP-TYPE = "F"
                   DISPLAY "EMPLOYEE TYPE: FULL-TIME"
               WHEN WS-EMP-TYPE = "P"
                   DISPLAY "EMPLOYEE TYPE: PART-TIME"
               WHEN WS-EMP-TYPE = "C"
                   DISPLAY "EMPLOYEE TYPE: CONTRACTOR"
               WHEN OTHER
                   DISPLAY "UNKNOWN EMPLOYEE TYPE"
           END-EVALUATE.
           
           DISPLAY "=== EVALUATE MULTIPLE CONDITIONS ===".
           EVALUATE WS-GRADE
               WHEN 1 THRU 3
                   DISPLAY "GRADE: JUNIOR LEVEL"
               WHEN 4 THRU 6
                   DISPLAY "GRADE: MID LEVEL"
               WHEN 7 THRU 9
                   DISPLAY "GRADE: SENIOR LEVEL"
               WHEN OTHER
                   DISPLAY "GRADE: INVALID"
           END-EVALUATE.
           
           DISPLAY "=== EVALUATE WITH LIST ===".
           MOVE 5 TO WS-MENU-OPTION.
           EVALUATE WS-MENU-OPTION
               WHEN 1, 3, 5
                   DISPLAY "ODD NUMBER SELECTED"
               WHEN 2, 4, 6
                   DISPLAY "EVEN NUMBER SELECTED"
               WHEN OTHER
                   DISPLAY "NUMBER OUT OF RANGE"
           END-EVALUATE.
           
           STOP RUN.