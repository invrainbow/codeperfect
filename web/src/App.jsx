import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { IconClockHour4, IconMenu2, IconTag, IconX } from "@tabler/icons";
import cx from "classnames";
import _ from "lodash";
import React from "react";
import { Helmet } from "react-helmet";
import {
  BrowserRouter,
  Link,
  Navigate,
  Outlet,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import "./index.css";

const CURRENT_BUILD = process.env.REACT_APP_BUILD_VERSION;
const CURRENT_BUILD_RELEASE_DATE = "September 16, 2023";

const isDev = process.env.REACT_APP_CPENV === "development";

const BASE_LINKS = {
  docs: "https://docs.codeperfect95.com",
  changelog: "https://docs.codeperfect95.com/changelog",
  github: "https://github.com/codeperfect95/codeperfect",
};

const DEV_LINKS = {
  docs: "http://localhost:3000",
  changelog: "http://localhost:3000/changelog",
};

const LINKS = {
  ...BASE_LINKS,
  ...(isDev ? DEV_LINKS : {}),
};

const DOWNLOADS = [
  {
    platform: "mac-x64",
    icon: AiFillApple,
    label: "macOS Intel",
  },
  {
    platform: "mac-arm",
    icon: AiFillApple,
    label: "macOS M1",
  },
];

function asset(path) {
  return isDev ? process.env.PUBLIC_URL + path : path;
}

function isExternalLink(href) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

function A({ children, href, newWindow, ...props }) {
  if (href && !isExternalLink(href) && !newWindow) {
    props.to = href;
    return <Link {...props}>{children}</Link>;
  }

  let useNewWindow;
  if (newWindow) {
    useNewWindow = true;
  } else if (newWindow === false) {
    useNewWindow = false;
  } else if (isExternalLink(href) && !href.startsWith("mailto:")) {
    useNewWindow = true;
  }

  if (useNewWindow) {
    props.target = "_blank";
  }
  props.href = href;
  return <a {...props}>{children}</a>;
}

function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}

function Home() {
  return (
    <div className="mx-auto w-full py-8 md:py-20">
      <div className="md:text-center font-bold text-5xl md:text-5xl px-4 mb-8 md:mb-12 text-black tracking-tight leading-tight">
        A fast, lightweight <span className="whitespace-nowrap">Go IDE</span>
      </div>

      <div className="mb-4 text-center mt-2 px-4 md:px-0">
        <A
          href={`${LINKS.changelog}/${CURRENT_BUILD}`}
          className={cx(
            "flex items-start md:items:center md:justify-center flex-col md:flex-row",
            "gap-1 md:gap-4 no-underline opacity-90 hover:opacity-100 font-ui"
          )}
        >
          <span className="inline-block">
            <span className="font-semibold text-sm text-neutral-700 flex gap-x-1.5 items-center">
              <Icon size={18} icon={IconTag} />
              <span>Build {CURRENT_BUILD}</span>
            </span>
          </span>
          <span className="inline-block">
            <span className="font-semibold text-sm text-neutral-700 gap-x-1.5 items-center flex">
              <Icon size={18} icon={IconClockHour4} />
              <span>{CURRENT_BUILD_RELEASE_DATE}</span>
            </span>
          </span>
        </A>
      </div>

      <div className="mt-4 md:text-center px-4">
        <p className="md:max-w-md mx-auto grid grid-cols-1 md:grid-cols-2 gap-2">
          {DOWNLOADS.map((it) => (
            <A
              href={`https://codeperfect95.s3.us-east-2.amazonaws.com/app/${it.platform}-${CURRENT_BUILD}.zip`}
              className="btn btn1 btn-block leading-none py-4 px-5 justify-center"
            >
              Download {it.label}
            </A>
          ))}
        </p>
      </div>

      <div className="max-w-[550px] mx-auto mt-12 md:mt-20 px-4">
        <p>
          CodePerfect was an experiment to try and build a faster IDE. It
          eschews the modern tech stack and is instead written from scratch in
          C/C++/OpenGL like a video game. It starts instantly, runs at 144 FPS,
          and has a near-zero latency experience.
        </p>
        <p>
          It comes with native full-featured code intelligence and integrated
          debugging with Delve. See more of the features{" "}
          <A href="/features">here</A>.
        </p>
        <p>
          It's no longer in active development, but is now{" "}
          <A href={LINKS.github}>open source</A> and available for free use.
        </p>
      </div>
    </div>
  );
}

const UNSORTED_FEATURE_LIST = [
  {
    name: "Autocomplete",
    body: (
      <>
        <p>
          <img src={asset("/feature-autocomplete.png")} alt="autocomplete" />
        </p>
        <p>
          Like any IDE, CodePerfect provides automatic completion to help you
          write code. Press <code>Ctrl+Space</code> to show all declared
          identifiers in the current scope, or press <code>.</code> after an
          identifier to see fields and methods.
        </p>
      </>
    ),
  },
  {
    name: "Command Palette",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-command-palette.png")}
            alt="command palette"
          />
        </p>
        <p>
          Every action in CodePerfect can be run as a command through the
          command palette. Just press <code>Cmd+K</code> anywhere to bring it
          up.
        </p>
        <p>
          The command palette supports fuzzy search so you can access commands
          quickly. It also displays the keyboard shortcut for future ease of
          use.
        </p>
      </>
    ),
  },
  {
    name: "Global Fuzzy Finder",
    body: (
      <>
        <p>
          <img src={asset("/feature-goto-file.png")} alt="goto file" />
        </p>
        <p>
          Use fuzzy search to jump to any file or symbol. Press{" "}
          <code>Cmd+P</code> any time to open the file finder, and{" "}
          <code>Cmd+T</code> to open the symbol finder.
        </p>
        <p>
          We know navigation is a critical part of the developer workflow, so we
          specially made sure it was lag-free. From opening the finder, to
          typing each key, to selecting a file, there's no latency at any step.
        </p>
      </>
    ),
  },
  {
    name: "Format File",
    body: (
      <>
        <p>
          CodePerfect allows you to format your file at any time, as well as the
          option to automatically format on save.
        </p>
        <p>
          We use our in-memory index to organize your imports, removing unused
          imports and detecting which imports are needed to resolve undeclared
          symbols. This yields greater accuracy and speed.
        </p>
      </>
    ),
  },
  {
    name: "Postfix Completion",
    body: (
      <>
        <p>
          <img src={asset("/feature-postfix.png")} alt="postfix" />
        </p>
        <p>
          CodePerfect provides several macros in the autocomplete menu after you
          type <code>.</code> after an expression.
        </p>
        <p>
          These macros range from the powerful <code>.check!</code>, which
          assigns variables to the expression's return values, checks whether{" "}
          <code>err != nil</code>, and returns the zero-value of the current
          function's return type if so; to the straightforward{" "}
          <code>.ifnotnil</code>, which expands <code>x.ifnotnil!</code> to
        </p>
        <pre>
          if x != nil &#123;{"\n  "}
          {"// "}cursor here{"\n"}&#125;
        </pre>
      </>
    ),
  },
  {
    name: "Jump to Definition",
    body: (
      <>
        <p>
          CodePerfect can jump to the definition of any declared symbol. Either
          move your cursor over it and press <code>Cmd+G</code> (or{" "}
          <code>gd</code> in Vim mode), or hold down <code>Cmd</code> and click
          it.
        </p>
      </>
    ),
  },
  {
    name: "Manage Interfaces",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-generate-implementation.png")}
            alt="generate implementation"
          />
        </p>
        <p>
          CodePerfect provides several facilities for navigating and working
          with interfaces:
        </p>
        <ul>
          <li>Given a type, list the interfaces it implements</li>
          <li>Given an interface, list the types that implement it</li>
          <li>Generate implementation of interface for type</li>
        </ul>
      </>
    ),
  },
  {
    name: "Build & Debug",
    body: (
      <>
        <p>
          CodePerfect is designed to make the edit-build-debug loop as seamless
          as possible.
        </p>
        <p>
          Trigger a build, jump to the first error, write a fix, jump to next
          error, fix, rebuild. Everything is done with ergonomic hotkeys. Error
          positions are preserved as you edit code. The entire experience is
          frictionless.
        </p>
        <p>
          When you're ready to debug, CodePerfect integrates with Delve to
          provide a seamless debugging experience.
        </p>
      </>
    ),
  },
  {
    name: "Global Live Search",
    body: (
      <>
        <p>
          Press <code>Cmd+Shift+F</code> to open project-wide search. Grepping a
          whole folder is fast enough on modern machines that we display results
          in realtime after each keystroke. It's like a faster, completely-local
          Algolia.
        </p>
        <p>
          When you see the result you want, use <code>Up</code> and{" "}
          <code>Down</code> to navigate results and <code>Enter</code> to
          select.
        </p>
        <p>
          We also support project-wide replace in the same window; press{" "}
          <code>Cmd+Shift+H</code>.
        </p>
      </>
    ),
  },
  {
    name: "Rename Anything",
    body: (
      <>
        <p>
          CodePerfect can rename almost anything that's declared within your
          project. Just hover over an identifier and run the <code>Rename</code>{" "}
          command (or press <code>F12</code>). This works on any identifier, not
          just the actual declaration. It also works on struct field names.
        </p>
      </>
    ),
  },
  {
    name: "Tree-Based Navigation",
    body: (
      <>
        <p>
          You can traverse your code by traversing its AST. Press{" "}
          <code>Ctrl+Alt+A</code> to enter tree-based navigation. Then,
        </p>
        <ul>
          <li>
            <code>Down</code> or <code>Right</code> to move to next sibling node
          </li>
          <li>
            <code>Up</code> or <code>Left</code> to move to previous sibling
            node
          </li>
          <li>
            <code>Shift+Down</code> or <code>Shift+Right</code> to move inward
            to child node
          </li>
          <li>
            <code>Shift+Up</code> or <code>Shift+Left</code> to move outward to
            parent node
          </li>
        </ul>
        <p>
          For Vim users, <code>h</code> <code>j</code> <code>k</code>{" "}
          <code>l</code> can be used instead of the arrow keys.
        </p>
      </>
    ),
  },
  {
    name: "Vim Keybindings",
    body: (
      <>
        <p>
          CodePerfect supports Vim keybindings out of the box. Go to{" "}
          <code>Tools</code> &gt; <code>Options</code> &gt;{" "}
          <code>Editor Settings</code> to enable them.
        </p>
        <p>
          Vim keybindings are a work in progress, but we currently support most
          of the commonly used commands, and the development team currently uses
          it for everyday text editing.
        </p>
      </>
    ),
  },
  {
    name: "Video Game Performance",
    body: (
      <>
        <p>
          CodePerfect was designed from the ground up with performance in mind.
          Instant startup, no latency, 144 frames a second, runs as smooth as a
          video game.
        </p>
      </>
    ),
  },
  {
    name: "Generate Function",
    body: (
      <>
        <p>
          Sometimes you're coding and you wish that a function or method
          existed, so you make a note to implement it later.
        </p>
        <p>
          With CodePerfect, you can write a function call, and CodePerfect can
          generate the function signature based on the types of the parameters.
          For instance, if you have
        </p>
        <pre>
          x := 0{"\n"}y := false{"\n"}calculate(x, y)
        </pre>
        <p>
          you can hover over <code>calculate</code> and run{" "}
          <code>Generate Function From Call</code>. It will generate
        </p>
        <pre>func calculate(v0 int, v1 bool)</pre>
      </>
    ),
  },
  {
    name: "Find References",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-find-references.png")}
            alt="find references"
          />
        </p>
        <p>
          Find References works as it does in other IDEs. Hover over the name
          and press <code>Cmd+Alt+R</code> or run <code>Find References</code>{" "}
          in the command palette.
        </p>
      </>
    ),
  },
  {
    name: "Manage Struct Tags",
    body: (
      <>
        <p>
          CodePerfect comes with commands to let you generate, add and remove
          tags from structs. Just search the command palette for{" "}
          <code>struct</code>.
        </p>
      </>
    ),
  },
];

const FEATURE_LIST = _.sortBy(UNSORTED_FEATURE_LIST, "name");

function Features() {
  const onScroll = (name) => {
    const elem = document.querySelector(`[data-feature-name="${name}"]`);
    if (elem) {
      elem.scrollIntoView({
        behavior: "smooth",
      });
    }
  };

  return (
    <div className="max-w-screen-lg flex mx-auto my-8 md:my-16 gap-12 features px-4">
      <div className="w-[175px] hidden md:block">
        <div className="sticky top-8">
          {FEATURE_LIST.map((it) => (
            <div className="mb-3 last:mb-0 leading-none">
              <button
                onClick={() => onScroll(it.name)}
                className="px-0 leading-none font-medium text-neutral-600 hover:text-neutral-900 text-left"
                key={it.name}
              >
                {it.name}
              </button>
            </div>
          ))}
        </div>
      </div>
      <div className="flex-1">
        <div className="px-4 md:px-0">
          <div className="title text-3xl md:text-5xl mb-4">Features</div>
          <div className="mb-16">
            This is a brief overview of the features inside CodePerfect. For a
            more complete and in-depth list, see the full{" "}
            <A href={LINKS.docs}>docs</A>.
          </div>
        </div>
        <div className="flex flex-col gap-12 md:gap-16 px-4 md:px-0">
          {FEATURE_LIST.map((it) => (
            <div data-feature-name={it.name} key={it.name}>
              <div className="mb-4 font-bold text-lg leading-none">
                {it.name}
              </div>
              {it.body}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

function ScrollToTop() {
  const { pathname, hash } = useLocation();

  React.useEffect(() => {
    if (!hash) {
      window.scrollTo(0, 0);
    }
  }, [pathname, hash]);

  return null;
}

function Logo({ onClick, hideText }) {
  return (
    <A
      href="/"
      className="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
      onClick={onClick}
    >
      <img
        alt="logo"
        className="w-auto h-8 inline-block mr-3"
        src={asset("/logo.png")}
      />
      {!hideText && (
        <span className="inline-block logo text-lg font-bold">CodePerfect</span>
      )}
    </A>
  );
}

function Layout() {
  const [showMenu, setShowMenu] = React.useState(false);

  React.useEffect(() => {
    const listener = (e) => {
      setShowMenu(false);
    };
    document.body.addEventListener("click", listener);
    return () => document.body.removeEventListener("click", listener);
  }, []);

  const links = [
    [LINKS.docs, "Docs"],
    [LINKS.changelog, "Changelog"],
    ["/features", "Features"],
    [LINKS.github, "Github"],
  ];

  return (
    <div className="flex flex-col min-h-screen">
      <div className="flex-grow">
        <ScrollToTop />

        <div className="p-4 md:p-4">
          <div className="flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
            <Logo />
            <div className="md:hidden relative">
              <Icon
                block
                onClick={(e) => {
                  setShowMenu(!showMenu);
                  e.stopPropagation();
                }}
                className="ml-2 text-3xl leading-none opacity-50"
                icon={IconMenu2}
              />
              {showMenu && (
                <div
                  className="bg-neutral-900 text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg z-50"
                  onClick={(e) => e.stopPropagation()}
                >
                  <button
                    onClick={() => setShowMenu(false)}
                    className="absolute z-50 top-4 right-4 w-8 flex items-center justify-center h-8 rounded-full bg-neutral-700 text-white"
                  >
                    <Icon icon={IconX} />
                  </button>
                  <div className="invert z-40 relative">
                    <Logo onClick={() => setShowMenu(false)} />
                  </div>
                  <div className="mt-2 flex flex-col md:flex-row md:items-center">
                    {links.map(([url, label]) => (
                      <A
                        key={url}
                        className="flex text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2 md:items-center"
                        onClick={() => setShowMenu(false)}
                        href={url}
                      >
                        {label}
                      </A>
                    ))}
                  </div>
                </div>
              )}
            </div>
            <div className="hidden md:flex items-center gap-x-8">
              {links.map(([url, label]) => (
                <A
                  key={url}
                  className="text-[95%] text-neutral-700 no-underline whitespace-nowrap hidden md:inline-flex"
                  href={url}
                >
                  {label}
                </A>
              ))}
            </div>
          </div>
        </div>
        <Outlet />
      </div>
      <div className="bg-white px-6 py-8">
        <div className="text-center">
          <div className="text-gray-500">
            <div className="opacity-50">
              <Logo hideText />
            </div>
            <div className="text-xs">
              &copy; {new Date().getFullYear()} CodePerfect
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function App() {
  return (
    <>
      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect</title>
      </Helmet>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route path="features" element={<Features />} />
            <Route path="/" element={<Home />} />
            <Route path="*" element={<Navigate to="/" />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </>
  );
}

export default App;
