%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
clean: 
	rm -rf $(OBJ) *.o *.out *~ core* $(BIN)
love: clean all
