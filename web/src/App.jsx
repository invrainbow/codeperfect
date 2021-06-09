import React from "react";
import {
  BrowserRouter as Router,
  Switch,
  Route,
  Link,
  useLocation,
} from "react-router-dom";
import { Helmet } from "react-helmet";
import cx from "classnames";
import _ from "lodash";
import { PlayIcon } from "@heroicons/react/solid";

// static assets
import ideScreenshotImage from "./ide.png";
import gif60fps from "./60fps.gif";
import gifVim from "./vim.gif";
import pngIntellisense from "./intellisense.png";
import pngVideoScreenshot from "./video-screenshot.png";

// constants
const IDE_NAME = "CodePerfect 95";
const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_YEAR = new Date().getFullYear();
const CURRENT_PRICE = 5;

function WallOfText({ title, children }) {
  return (
    <div className="wall-of-text border-t border-b md:border border-gray-700 py-5 md:p-12 md:rounded-sm leading-normal">
      <div className="md:max-w-2xl md:mx-auto">
        {title && <Title>{title}</Title>}
        {children}
      </div>
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-xl font-bold text-gray-200" {...props}>
      {children}
    </h2>
  );
}

function NiceImage({ className, ...props }) {
  return (
    <div className={cx("flex items-center justify-center", className)}>
      <img alt="" className="overflow-hidden nice-image w-full" {...props} />
    </div>
  );
}

function Section({ className, ...props }) {
  return (
    <div
      className={cx(
        "mb-24 md:gap-14 flex flex-col md:flex-row md:mb-32 md:items-center",
        className
      )}
      {...props}
    />
  );
}

const IDE_FEATURES = _.shuffle([
  "Auto Completion",
  "Integrated Debugger",
  "Integrated Build",
  "Auto Format",
  "Parameter Hints",
  "Go To Definition",
  "Vim Keybindings",
  "Fuzzy File Search",
  "GPU-Based Renderer",
  "Highly Optimized Core",
  // "Debug Tests",
  "Instant Startup",
  "Syntax Highlighting",
  // "Navigate Errors",
]);

function Home() {
  return (
    <div>
      <div className="mt-8 mb-24 sm:my-24 md:my-32">
        <div className="leading-tight text-center mb-6 md:mb-12 text-4xl md:text-5xl font-light text-white">
          A Blazing Fast IDE for Go
        </div>
        <img
          alt=""
          className="nice-image max-w-auto"
          src={ideScreenshotImage}
        />
      </div>

      <Section className="md:block lg:flex">
        <div className="leading-relaxed mb-4 md:flex md:flex-row lg:block md:gap-8 md:mb-4 lg:w-1/3">
          <div className="text-2xl mb-4 text-gray-200 md:w-4/12 lg:w-full font-light">
            No Electron. No JavaScript. No garbage collection.
          </div>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            {IDE_NAME} is written in C++ and designed to run at 144 FPS. Every
            keystroke responds instantly.
          </p>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            Modern laptop CPUs perform billions of ops/second. None of your apps
            should ever lag. {IDE_NAME} never does.
          </p>
        </div>
        <NiceImage src={gif60fps} className="lg:w-2/3" />
      </Section>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed md:w-5/12">
          <div className="text-2xl mb-4 text-gray-200 font-light">
            A batteries-included IDE, as fast as Vim.
          </div>
          <p>
            Today, IDEs are powerful but slow, while Vim is fast but limited.
          </p>
          <p>
            {IDE_NAME} brings the best of both worlds: everything you need to
            effectively develop in Go, at lightning speed.
          </p>
          <p>
            No more waiting on GoLand's garbage collector. No more hacking Vim
            plugins together.
          </p>
        </div>
        <div className="md:w-7/12 mt-8 md:mt-0 grid grid-cols-3 sm:grid-cols-4 md:grid-cols-4 lg:grid-cols-4 gap-3 md:gap-6 select-none">
          {IDE_FEATURES.map((feature, i) => (
            <div
              key={i}
              className="font-semibold h-16 md:h-20 lg:h-20 rounded-lg text-gray-400 hover:text-gray-300 text-center flex justify-center items-center text-xs lg:text-sm p-4 transform hover:scale-110 transition-all shadow-sm"
              style={{ background: "#1d1d1d", border: "solid 1px #555" }}
            >
              <span>{feature}</span>
            </div>
          ))}
        </div>
      </Section>

      <div className="block md:hidden">
        <Section>
          <div className="leading-relaxed md:flex md:flex-row md:gap-8 md:mb-12">
            <div className="text-2xl mb-4 text-gray-200 md:w-1/3 font-light">
              A toolkit that understands Go as a first language.
            </div>
            <p className="md:w-1/3 md:mt-0">
              {IDE_NAME}'s intellisense is hand-written, extensively tested, and
              designed for speed and reliability.
            </p>
            <p className="md:w-1/3 md:mt-0">
              No more language server madness. No more sending a JSON packet
              over a socket every keystroke. No more restarting your IDE because
              gopls crashed.
            </p>
          </div>
        </Section>
      </div>

      <div className="hidden md:block relative rounded-md overflow-hidden border-gray-500 border mb-24 md:mb-32">
        <div
          className="leading-relaxed md:gap-8 absolute top-8 left-0 w-5/12 z-20 p-8 drop-shadow rounded-tr-md rounded-br-md"
          style={{ background: "rgba(0, 0, 0, 0.5)" }}
        >
          <div className="text-2xl mb-4 text-gray-200 font-light">
            A toolkit that understands Go as a first language.
          </div>
          <p className="">
            {IDE_NAME}'s intellisense is hand-written, extensively tested, and
            designed for speed and reliability.
          </p>
          <p className="">
            No more language server madness. No more sending a JSON packet over
            a socket every keystroke. No more restarting your IDE because gopls
            crashed.
          </p>
        </div>
        <img
          src={pngIntellisense}
          alt=""
          className="relative z-10 w-full opacity-40"
        />
      </div>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed mb-4 md:mb-0 md:w-5/12">
          <div className="text-2xl mb-4 text-gray-200 font-light">
            Complete Vim keybindings, out of the box.
          </div>
          <p>
            Our Vim keybindings aren't a plugin added as an afterthought.
            They're built into the core of the application and designed to work
            seamlessly with everything else.
          </p>
          <p>
            We're big Vim users ourselves, and designed {IDE_NAME} with
            first-class Vim support from the beginning. It basically feels like
            real Vim.
          </p>
        </div>
        <NiceImage src={gifVim} className="md:w-7/12" />
      </Section>

      <p className="text-center mb-6">
        Watch a demo of {IDE_NAME} being used to solve{" "}
        <a target="_blank" rel="noreferrer" href="https://cryptopals.com">
          Cryptopals
        </a>
        .
      </p>

      <div className="mx-auto mb-16 md:mb-32 md:w-3/4">
        <a
          className="relative border shadow-md group"
          style={{ borderColor: "#888" }}
          rel="noreferrer"
          target="_blank"
          href="https://www.youtube.com/watch?v=9Q0YFHHLG-g"
        >
          <div className="absolute z-10 flex justify-center items-center w-full h-full">
            <PlayIcon className="group-hover:opacity-70 h-16 w-16 text-white drop-shadow opacity-50" />
          </div>
          <img
            alt=""
            src={pngVideoScreenshot}
            className="relative z-0 block opacity-30"
          />
        </a>
      </div>
    </div>
  );
}

function Terms() {
  return (
    <WallOfText title="Terms of Service">
      <p>
        This website provides you with information about the IDE and a means for
        you to subscribe to our services, which allow you to use the IDE for as
        long as your subscription is active.
      </p>
      <p>
        The IDE is an application that lets you write Go applications. In
        exchange for paying a monthly rate, we provide you with a license to use
        it.
      </p>
    </WallOfText>
  );
}

function Privacy() {
  return (
    <WallOfText title="Privacy Policy">
      <p>
        When you sign up, we collect your name, email, and credit card
        information. We use this information to bill you and send you emails
        with updates about your payment status (for example, if your card
        fails).
      </p>
      <p>
        When you sign up for our newsletter we collect your name and email. We
        use this to send you updates about new product features.
      </p>
      <p>
        The IDE contacts the server to authenticate your license key and to
        install automatic updates. This exposes your IP address to us. We won't
        share it with anyone, unless required to by law.
      </p>
    </WallOfText>
  );
}

function Beta() {
  return (
    <div
      className="border-t border-b py-4 md:py-12"
      style={{ borderColor: "#282828" }}
    >
      <div
        className="relative md:grid md:space-x-12"
        style={{ "grid-template-columns": "33% 67%" }}
      >
        <div
          className="sticky top-4 mb-4 md:top-4 md:self-start border border-gray-700 rounded-md shadow-md overflow-hidden"
          style={{ background: "#151515" }}
        >
          <div className="text-sm text-center md:text-lg md:border-b border-gray-700 pb-2 md:pt-4 p-4 md:p-4">
            <b className="text-white">${CURRENT_PRICE}/month</b> per seat
          </div>

          <div
            className="text-center md:border-b md:border-gray-700 pt-0 md:pt-4 p-4"
            style={{ background: "#111" }}
          >
            <a
              className="main-button text-md md:text-lg py-2 px-4 md:px-8 md:py-3 w-auto block"
              href="https://airtable.com/shraN38Z2jqQJVqbk"
              target="_blank"
              rel="noreferrer"
            >
              Join Beta
            </a>
            <p className="text-left md:block mt-4 text-sm">
              Note: We are rapidly adding support for various things -
              macOS/Linux is high on our list. So if you're interested but your
              setup isn't supported, if you fill out the form anyway, we'll
              notify you when we roll out support.
            </p>
          </div>
        </div>

        <div>
          <Title>Before you sign up...</Title>
          <p>
            {IDE_NAME} is still in early beta. We're releasing it now because we
            use it every day ourselves, and realized we were getting more than $
            {CURRENT_PRICE} of monthly utility from it. That said, there are
            currently several large limitations:
          </p>
          <ul className="thick-list">
            <li>Windows only (Windows 10).</li>
            <li>Go 1.13+ only.</li>
            <li>
              Your project must be module-aware, and consist of a single module,
              located at your project's root folder. I.e.{" "}
              <code>go list -mod=mod -m all</code> must work from your root
              folder.
            </li>
            <li>
              Editing via Vim keys only. Arrow keys, clicking, and scrolling
              don't work in the editor yet.
            </li>
            <li>No project-wide Search/Search and Replace.</li>
            <li>No built-in terminal.</li>
            <li>No WSL support.</li>
            <li>No support for symlinks (undefined behavior).</li>
            <li>
              Slow when opening extremely large workspaces (e.g. the{" "}
              <a href="https://github.com/kubernetes/kubernetes">kubernetes</a>{" "}
              repo).
            </li>
            <li>
              No Vim commands. Importantly, no <code>:%s/search/replace</code>.
              (<code>/search</code> works.)
            </li>
            <li>
              Vim macros work, but when repeated (e.g. <code>200@@</code>) they
              start to slow down.
            </li>
          </ul>
          <p>
            Obviously, these limitations are not part of our eventual vision.
            We're working hard to remove them. But in the meantime, {IDE_NAME}{" "}
            is ready for day-to-day, bread-and-butter Go programming. Everyone
            on the dev team uses it. The following core features are working:
          </p>
          <ul className="thick-list">
            <li>
              Automatically index your code and its dependencies in real-time
            </li>
            <li>Autocomplete (member &amp; keyword)</li>
            <li>Parameter hints (show function signature on call)</li>
            <li>Jump to definition</li>
            <li>
              Auto format on save (with <code>goimports</code>)
            </li>
            <li>Vim keybindings</li>
            <li>
              Debugging with call stack, breakpoints, local variables, and
              watches
            </li>
            <li>Debug tests (package-wide &amp; individual tests)</li>
            <li>Building, navigating between errors</li>
            <li>Fuzzy file picker</li>
            <li>Extremely optimized core</li>
          </ul>
          <p>
            If this sounds compatible with your setup, {IDE_NAME} will allow you
            to start developing Go programs at lightning speed today. We're also{" "}
            <a
              target="_blank"
              rel="noreferrer"
              href="https://www.notion.so/53c8fbc9b3074236b8b7649f4593b6c7?v=d1af3ea58cd346eeba9f4eb65d801a89"
            >
              steadily developing new features
            </a>
            .
          </p>
        </div>
      </div>
    </div>
  );
}

function ScrollToTop() {
  const { pathname } = useLocation();

  React.useEffect(() => {
    window.scrollTo(0, 0);
  }, [pathname]);

  return null;
}

function App() {
  return (
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>{IDE_NAME}</title>
      </Helmet>

      <div className="p-6 md:p-12 text-gray-400 w-full lg:max-w-screen-xl lg:mx-auto">
        <div className="mx-auto pb-4 flex justify-between items-center">
          <Link
            to="/"
            className="font-bold text-white no-underline whitespace-nowrap"
          >
            {IDE_NAME}
          </Link>
          <div className="flex items-baseline space-x-6">
            <a
              className="no-underline text-sm font-semibold text-gray-300 hidden sm:inline-block"
              href="https://www.notion.so/CodePerfect-95-FAQs-9f227faf607e47c19e33a44e82a6a8a9"
              target="_blank"
              rel="noreferrer"
            >
              FAQ
            </a>
            <a
              target="_blank"
              rel="noreferrer"
              className="no-underline text-sm font-semibold text-gray-300 hidden sm:inline-block"
              href="https://www.notion.so/The-CodePerfect-95-Philosophy-5014f8f69cad4ef6b964ebc25fdc0b31"
            >
              Philosophy
            </a>
            <a
              target="_blank"
              rel="noreferrer"
              className="no-underline text-sm font-semibold text-gray-300 hidden sm:inline-block"
              href="https://www.notion.so/53c8fbc9b3074236b8b7649f4593b6c7?v=d1af3ea58cd346eeba9f4eb65d801a89"
            >
              Roadmap
            </a>
            <Link className="main-button whitespace-nowrap" to="/beta">
              Join Beta
            </Link>
          </div>
        </div>
        <div className="">
          <Switch>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Privacy />
            </Route>
            <Route path="/beta">
              <Beta />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
          </Switch>
        </div>
        <div className="text-sm pt-4">
          <div className="lg:max-w-screen-xl lg:mx-auto flex flex-col sm:flex-row justify-between">
            <div className="text-gray-400">
              &copy; {CURRENT_YEAR} {IDE_NAME}
            </div>
            <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-8 mt-2 sm:mt-0">
              <div className="text-left sm:text-right">
                <div className="mb-1">
                  <Link to="/beta" className="text-gray-400 no-underline">
                    Join Beta
                  </Link>
                </div>
                <div className="mb-1">
                  <a
                    target="_blank"
                    rel="noreferrer"
                    className="text-gray-400 no-underline"
                    href="https://www.notion.so/The-CodePerfect-95-Philosophy-5014f8f69cad4ef6b964ebc25fdc0b31"
                  >
                    Philosophy
                  </a>
                </div>
                <div className="mb-1">
                  <a
                    target="_blank"
                    rel="noreferrer"
                    className="text-gray-400 no-underline"
                    href="https://www.notion.so/53c8fbc9b3074236b8b7649f4593b6c7?v=d1af3ea58cd346eeba9f4eb65d801a89"
                  >
                    Roadmap
                  </a>
                </div>
                <div className="mb-1">
                  <a
                    rel="noreferrer"
                    target="_blank"
                    href="https://www.notion.so/CodePerfect-95-FAQs-9f227faf607e47c19e33a44e82a6a8a9"
                    className="text-gray-400 no-underline"
                  >
                    FAQ
                  </a>
                </div>
              </div>
              <div className="text-left sm:text-right">
                <div className="mb-1">
                  <Link to="/terms" className="text-gray-400 no-underline">
                    Terms of Service
                  </Link>
                </div>
                <div className="mb-1">
                  <Link className="text-gray-400 no-underline" to="/privacy">
                    Privacy Policy
                  </Link>
                </div>
                <div className="mb-1">
                  <a
                    className="text-gray-400 no-underline"
                    href={`mailto:${SUPPORT_EMAIL}`}
                  >
                    Contact
                  </a>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Router>
  );
}

export default App;
