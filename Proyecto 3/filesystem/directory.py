class Directory:
    def __init__(self, name, parent=None):
        self.name = name
        self.parent = parent
        self.subdirs = {}
        self.files = {}

    def add_subdir(self, dirname):
        if dirname in self.subdirs:
            raise Exception("Ya existe un subdirectorio con ese nombre.")
        new_dir = Directory(dirname, parent=self)
        self.subdirs[dirname] = new_dir
        return new_dir
