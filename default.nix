# Run Beyond All Reason (BAR) using AppImage.
# Note: it takes a minute to start for the first time. You may see a blank screen.

let
  # Pin nixpkgs.
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-25.05";
  pkgs = import nixpkgs { config = { allowUnfree = true; }; overlays = []; };

  vscode_ext = pkgs.vscode-with-extensions.override {
    vscodeExtensions = with pkgs.vscode-extensions; [
      platformio.platformio-vscode-ide
      jnoortheen.nix-ide
      ms-vscode.cpptools
      ms-vscode.cpptools-extension-pack
    ];
  };

  python_ext = pkgs.python312.withPackages (python-pkgs: [
    python-pkgs.bleak
  ]);
in
pkgs.mkShellNoCC {
  name = "dev";
  packages = with pkgs; [
    platformio
    vscode_ext
    python_ext
  ];

}
