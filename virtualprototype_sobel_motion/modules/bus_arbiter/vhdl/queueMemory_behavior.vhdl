ARCHITECTURE noPlatformSpecific OF queueMemory IS

  TYPE MEMORY IS ARRAY(NATURAL RANGE<>) OF std_logic_vector( 31 DOWNTO 0 );
  
  SIGNAL mem : MEMORY(31 DOWNTO 0); 

BEGIN

  makeMem : PROCESS ( writeClock ) IS
  BEGIN
    IF (rising_edge(writeClock)) THEN
      IF (writeEnable = '1') THEN
        mem(TO_INTEGER(UNSIGNED(writeAddress))) <= writeData;
      END IF;
      dataReadPort <= mem(TO_INTEGER(UNSIGNED(readAddress)));
    END IF;
  END PROCESS makeMem;
  
END noPlatformSpecific;
