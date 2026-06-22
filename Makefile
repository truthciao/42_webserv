# ============================================================================== #
#                                    VARIABLES                                   #
# ============================================================================== #

# Compiler and Flags
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98

# Project Name
NAME		= webserv

# Directories
SRC_DIR		= srcs
OBJ_DIR		= objs
INC_DIR		= includes

# Source Files
SRCS_FILES	= main.cpp \
			  core/Server.cpp \
			  core/Client.cpp \
			  core/Logger.cpp \
			  core/CgiHandler.cpp \
			  http/Request.cpp \
			  http/Response.cpp \
			  http/Router.cpp \
			  http/Autoindex.cpp \
			  http/MultipartParser.cpp \
			  config/Config.cpp \
			  config/ConfigParser.cpp

# Prepend directory paths to source files
SRCS		= $(addprefix $(SRC_DIR)/, $(SRCS_FILES))

# Generate object file names by replacing .cpp with .o and prepending obj dir
OBJS		= $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Include path flag
INC_FLAGS	= -I $(INC_DIR)

# Dependency files for header tracking
DEPS		= $(OBJS:.o=.d)

# Colors for output
GREEN		= \033[0;32m
YELLOW		= \033[0;33m
RED			= \033[0;31m
RESET		= \033[0m

# ============================================================================== #
#                                     RULES                                      #
# ============================================================================== #

# Default rule
all: $(NAME)

# Rule to link the executable
$(NAME): $(OBJS)
	@printf "$(YELLOW)Linking objects to create executable: $(NAME)...$(RESET)\n"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@printf "$(GREEN)Executable $(NAME) created successfully!$(RESET)\n"

# Rule to compile .cpp files into .o files
# This also creates dependency files (.d) for header tracking
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@printf "$(GREEN)Compiling $<...$(RESET)\n"
	@$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@ -MMD

# Clean up object files and dependency files
clean:
	@printf "$(RED)Cleaning object files...$(RESET)\n"
	@rm -rf $(OBJ_DIR)

# Full clean: remove objects and the final executable
fclean: clean
	@printf "$(RED)Cleaning executable...$(RESET)\n"
	@rm -f $(NAME)

# Rebuild all
re: fclean all

# Run with valgrind to check for fd/memory leaks
# --track-fds=yes : reports any fd still open at exit
# --leak-check=full : full heap leak report
valgrind: $(NAME)
	valgrind --track-fds=yes --leak-check=full --show-leak-kinds=all ./$(NAME)

.PHONY: all clean fclean re valgrind

# Include dependency files. The `-` before include ignores errors if files don't exist.
-include $(DEPS)
