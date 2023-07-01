CXX=mpiicc

OBJS=test.o mpistream.o

test.x: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp mpistream.hpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) -c $<

clean:
	rm -rf $(OBJS)