NAME    = webserv
CXX     = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

SRCS    = main.cpp Client.cpp Server.cpp
OBJS    = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

# Run with valgrind to check for fd/memory leaks
# --track-fds=yes : reports any fd still open at exit
# --leak-check=full : full heap leak report
valgrind: $(NAME)
	valgrind --track-fds=yes --leak-check=full --show-leak-kinds=all ./$(NAME)

.PHONY: all clean fclean re valgrind
