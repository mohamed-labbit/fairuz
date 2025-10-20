/*
def parse():
    stack = [(0, None, None)]  # (state, symbol, ast_node)
    pos = 0
    
    while True:
        state = stack[-1][0]
        token = tokens[pos] if pos < len(tokens) else EOF_TOKEN
        action = ACTION[state][token.type]
        
        if action == ACCEPT:
            return stack[-1][2]  # Return AST root
            
        elif action.type == SHIFT:
            #Push token and new state
            ast_node = create_leaf_node(token)
            stack.append((action.state, token.type, ast_node))
            pos += 1
            
        elif action.type == REDUCE:
            production = action.production  # A → β
            beta_len = len(production.rhs)

            #Pop | β | items from stack
            children = []
            for _ in range(beta_len):
                _, _, child_ast = stack.pop()
                children.insert(0, child_ast)

            #Get goto state
            current_state = stack[-1][0]
            next_state = GOTO[current_state][production.lhs]

            #Create AST node for this production
            ast_node = create_ast_node(production.lhs, children)

            #Push non - terminal and new state
            stack.append((next_state, production.lhs, ast_node))
            
        else:
            raise SyntaxError(f"Unexpected token {token}")
*/