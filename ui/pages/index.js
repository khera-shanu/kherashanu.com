import Head from 'next/head';
import { useState } from 'react';

import Terminal, { ColorMode, TerminalOutput } from 'react-terminal-ui';

export default function Home() {
  const default_txt = `
  Following commands are supported:

  - ll
  - cd <dirname>
  - cat <filename>
  - wc <filename>
  - pwd
  - whoami
  - clear
  - vim
  - rm
`;

  const [cwd, setCWD] = useState('/');
  const [output, setOutput] = useState([default_txt]);

  // To Do - Use a tree to implement commands more accurately
  // For example - cat blogs/../info.txt should work

  const dir_tree = {
    '/': {
      fname: 'root',
      type: 'dir',
      permissions: 'drwxr-xr-x',
      path: '/',
      parent: null,
      children: [
        {
          fname: 'info.txt',
          type: 'file',
          permissions: '-rw-r--r--',
          content: `
As a senior software engineer at Nextroll Inc, I have dedicated my career to the art and science of computer programming.
With a background in hard-core Physics, Mathematics, and Computer Science, I have developed a deep understanding of the underlying principles that drive technology forward.
I am self-taught, with a natural aptitude for problem-solving and a desire to constantly push the limits of what is possible.

Over the years, I have honed my skills in a wide range of programming languages, including Python, JS, C, Erlang, Go, Rust, and Haskell.
While I have a diverse skill set, I am particularly drawn to the elegance and simplicity of Python, as well as the raw power of JS.
I have a distaste for languages such as Java, C#, and PHP, which I find to be clunky and cumbersome.

In addition to my full-time job, I am also the part-time CTO for my sister's fashion startup, AamomiFashion.
In this role, I have had the opportunity to apply my technical expertise to the world of e-commerce, building a platform from the ground up that is both functional and aesthetically pleasing.

Throughout my career, I have had the privilege of working in a variety of industries, including Telecom, Fintech, Education NGO, Edutech, and Sales-tech.
Each of these experiences has provided me with a unique perspective on the role of technology in the modern world, and has allowed me to develop a well-rounded skill set that is applicable to a wide range of projects.

As a pragmatic engineer, I have always had a passion for teaching and helping others to achieve their full potential.
Whether it is through my work as a mentor or as a speaker at industry events, I have always found great fulfillment in sharing my knowledge and expertise with others.
I am a strong believer in the power of education, and I am constantly seeking out opportunities to learn and grow myself.

Currently, I am not actively seeking a new job, but I am open to offers that I can't refuse.
If you are a company or organization that is looking for someone with a unique combination of technical skills and a passion for teaching, I would love to hear from you.

One last thing! I am also the founder of Giganoto.com\n`,
          path: '/info.txt',
          parent: '/',
        },
        {
          fname: 'blogs',
          type: 'dir',
          permissions: 'drwxr-xr-x',
          path: '/blogs',
          parent: '/',
          children: [
            {
              fname: 'updates.md',
              type: 'file',
              permissions: '-rw-r--r--',
              content: `## To Do`,
              path: '/blogs/updates.md',
              parent: '/blogs',
            },
          ],
        },
      ],
    },
  };

  const handleInput = (value) => {
    let new_output = '';
    if (value) {
      const cmd_args = value.split(' ').map((x) => x.toLowerCase());
      const cmd = cmd_args[0];
      const args = cmd_args.splice(1);
      switch (cmd) {
        case "clear":
          setOutput([])
          break;
        case "rm":
          new_output = `${cwd} $> You need to sudo for this command\n`;
          break;
        case "vim":
          new_output = `${cwd} $> You need to sudo for this command\n`;
          break;
        case "sudo":
          new_output = `${cwd} $> You need admin login for this command\n`;
          break;
        case "pwd":
          new_output = `${cwd}\n`;
          break;
        case 'll':
          const files = getFilesFromCWD()
            .map(({ fname, permissions }) => `${permissions} ${fname}`)
            .join('\n');
          new_output = files + '\n';
          break;
        case 'cd':
          if (args.length === 0) {
            new_output =
              'Please mention a directory name after the `cd` command\n';
            break;
          } else {
            const path = args[0];
            let full_path = cwd + path;
            if (path.startsWith('/')) {
              // Absolute path specified
              full_path = path;
            } else if (path.startsWith('../')) {
              // Go up one level in the tree
              const current_dir = dir_tree[cwd];
              full_path = current_dir.parent + '/' + path.substring(3);
            }
            const dir = getFileFromPath(full_path);
            if (!dir) {
              new_output = 'No such directory in the current directory.\n';
              break;
            }
            if (dir.type === 'file') {
              new_output = "You can't cd into a file.\n";
              break;
            } else {
              setCWD(full_path);
              break;
            }
          }
        case 'cat':
          if (args.length === 0) {
            new_output = 'Please mention a file name after the `cat` command\n';
            break;
          } else {
            const path = args[0];
            if (path.startsWith('/')) {
              // Absolute path specified
              const file = getFileFromPath(path);
              if (!file) {
                new_output = 'No such file in the current directory.\n';
                break;
              }
              if (file.type === 'dir') {
                new_output = "You can't cat a directory.\n";
                break;
              } else {
                new_output = file.content;
                break;
              }
            } else {
              // Relative path specified
              let full_path = cwd + '/' + path;
              if (path.startsWith('../')) {
                // Go up one level in the tree
                const current_dir = dir_tree[cwd];
                full_path = current_dir.parent + '/' + path.substring(3);
              }
              const file = getFileFromPath(full_path);
              if (!file) {
                new_output = 'No such file in the current directory.\n';
                break;
              }
              if (file.type === 'dir') {
                new_output = "You can't cat a directory.\n";
                break;
              } else {
                new_output = file.content;
                break;
              }
            }
          }
          break;
        case 'wc':
          if (args.length === 0) {
            new_output = 'Please mention a file name after the `wc` command\n';
            break;
          } else {
            const path = args[0];
            if (path.startsWith('/')) {
              // Absolute path specified
              const file = getFileFromPath(path);
              if (!file) {
                new_output = 'No such file in the current directory.\n';
                break;
              }
              if (file.type === 'dir') {
                new_output = "You can't wc a directory.\n";
                break;
              } else {
                const lines = file.content.split('\n').length;
                new_output = `${lines} ${path}\n`;
                break;
              }
            } else {
              // Relative path specified
              let full_path = cwd + '/' + path;
              if (path.startsWith('../')) {
                // Go up one level in the tree
                const current_dir = dir_tree[cwd];
                full_path = current_dir.parent + '/' + path.substring(3);
              }
              const file = getFileFromPath(full_path);
              if (!file) {
                new_output = 'No such file in the current directory.\n';
                break;
              }
              if (file.type === 'dir') {
                new_output = "You can't wc a directory.\n";
                break;
              } else {
                const lines = file.content.split('\n').length;
                new_output = `${lines} ${path}\n`;
                break;
              }
            }
          }
          break;
        // Other commands...
      }
      if(cmd !== "clear") setOutput(output.concat([`${cwd} $> ${value}\n`, new_output]));
    }
  };

  function getFilesFromCWD() {
    return dir_tree[cwd].children;
  }

  // Helper function to retrieve a file from the directory tree using its full path
  function getFileFromPath(path) {
    const path_segments = path.split('/').filter((x) => x !== '');
    let curr_node = dir_tree['/'];
    for (const segment of path_segments) {
      const children = curr_node.children;
      if (!children) return null;
      const matching_child = children.find((x) => x.fname === segment);
      if (!matching_child) return null;
      curr_node = matching_child;
    }
    return curr_node;
  }

  return (
    <>
      <Head>
        <title>Create Next App</title>
        <meta name="description" content="Generated by create next app" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <link rel="icon" href="/favicon.ico" />
      </Head>
      <main>
        <Terminal
          name="Khera Shanu CLI"
          onInput={handleInput}
          prompt={`${cwd} $>`}
        >
          <TerminalOutput>{output}</TerminalOutput>
        </Terminal>
      </main>
    </>
  );
}
