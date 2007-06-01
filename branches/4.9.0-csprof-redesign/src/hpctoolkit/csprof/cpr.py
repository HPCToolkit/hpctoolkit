import cmd
import readline

class CallPathRefiner(cmd.Cmd):
    def __init__(self, tree):
        cmd.Cmd.__init__(self)

        self.call_tree = tree
        self.current_refinement = []

    # immediate downward refinements
    def do_down_refine(self, line):
        pass

    # extended downward refinements
    def do_xdown_refine(self, line):
        pass

    # immediate upward refinements
    def do_up_refine(self, line):
        pass

    # extended upward refinments
    def do_xup_refine(self, line):
        pass
    
    # grouping nodes for clarity
    def do_group(self, line):
        pass

    # removing groups if necessary
    def do_ungroup(self, line):
        pass

    def do_list_groups(self, line):
        pass
