# Shakespeare Programming Language for VS Code

Syntax highlighting for `.spl` plays: acts and scenes, stage directions, speaker labels,
the SPL sentence keywords (assignment questions, `If so`, gotos, `Speak thy mind!`,
`Remember`/`Recall`), arithmetic phrases, pronouns and `nothing`.

Install from a checkout of the repository by linking the folder into your extensions:

    ln -s "$PWD/editors/vscode" ~/.vscode/extensions/spl

then reload the window (or `code --install-extension` a `.vsix` built with `npx vsce package`).
