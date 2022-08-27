clk=0
while true;
do echo clock $clk
	 sleep 1
	 clk=$(($clk+1))
done
