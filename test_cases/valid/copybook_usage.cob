       IDENTIFICATION DIVISION.
       PROGRAM-ID. COPY-BOOK-TEST.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Simulating a COPY BOOK inline for testing purposes
      * In real scenarios: COPY EMPLOYEE-RECORD.
       01  WS-EMPLOYEE-DATA.
           05  WS-EMP-ID       PIC 9(5).
           05  WS-EMP-FIRST    PIC X(15).
           05  WS-EMP-LAST     PIC X(20).
           05  WS-EMP-HIRE-DATE.
               10  WS-HIRE-YEAR    PIC 9(4).
               10  WS-HIRE-MONTH   PIC 9(2).
               10  WS-HIRE-DAY     PIC 9(2).
       01  WS-COUNTER        PIC 9(3) VALUE 0.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           MOVE 5001 TO WS-EMP-ID
           MOVE "ROBERT" TO WS-EMP-FIRST
           MOVE "BROWNING" TO WS-EMP-LAST
           MOVE 2023 TO WS-HIRE-YEAR
           MOVE 06 TO WS-HIRE-MONTH
           MOVE 15 TO WS-HIRE-DAY
           
           ADD 1 TO WS-COUNTER
           
           DISPLAY "EMPLOYEE RECORD CREATED"
           DISPLAY "ID: " WS-EMP-ID
           DISPLAY "NAME: " WS-EMP-FIRST " " WS-EMP-LAST
           DISPLAY "HIRED: " WS-HIRE-MONTH "/" 
                   WS-HIRE-DAY "/" WS-HIRE-YEAR
           DISPLAY "TOTAL PROCESSED: " WS-COUNTER.
           STOP RUN.
