# Slang Language Support

VS Code syntax highlighting support for Slang (`.sl`) files.

## Current Features

- TextMate-based syntax highlighting
- Language configuration (comments, brackets, auto-closing pairs)

## Development Setup

```bash
npm install
```

Press `F5` in VS Code to launch an Extension Development Host.

To build and install the extension, use

````
npm install -g vsce
vcse package
```
and install the resulting `slang-language-support-X.Y.Z.vsix` file in VS code.

## Notes

- This extension currently provides highlighting only.
- Semantic features (type-aware highlighting, go-to-definition, diagnostics) are not implemented yet.
````
