import React from "react";
import { BrowserRouter as Router, Switch, Route, Link } from "react-router-dom";
import ideScreenshotImage from "./ide.png";
import { Helmet } from "react-helmet";

import bear1Image from "./bear1.jpg";
import bear2Image from "./bear2.jpg";
import bear3Image from "./bear3.jpg";
import bear4Image from "./bear4.jpg";

const IDE_NAME = "CodePerfect 95";

const CURRENT_YEAR = new Date().getFullYear();

function Bear({ index }) {
  const BEAR_URLS = [bear1Image, bear2Image, bear3Image, bear4Image];
  const url = BEAR_URLS[index % BEAR_URLS.length];

  return (
    <div
      className="overflow-hidden rounded-md border-solid border-8 border-gray-100 h-80 bg-center bg-cover bg-no-repeat"
      style={{ backgroundImage: `url('${url}')` }}
    />
  );
}

function Section({ bearIndex, children }) {
  return (
    <div className="space-x-14 flex items-center mb-32">
      {bearIndex % 2 !== 0 && (
        <div className="w-3/5">
          <Bear index={bearIndex} />
        </div>
      )}
      <div className="w-2/5 leading-relaxed">{children}</div>
      {bearIndex % 2 === 0 && (
        <div className="w-3/5">
          <Bear index={bearIndex} />
        </div>
      )}
    </div>
  );
}

function Home() {
  return (
    <div>
      <div className="my-28">
        <div className="text-center mb-12 text-4xl font-bold">
          A Blazing Fast Golang IDE
        </div>
        <img alt="" className="max-w-auto" src={ideScreenshotImage} />
      </div>

      <Section bearIndex={0}>
        <div className="text-2xl mb-4">
          No Electron. No JavaScript. No garbage collection.
        </div>
        <p>
          {IDE_NAME} is written in C++ and designed to run at 144 FPS. Every
          keystroke responds instantly.
        </p>
        <p>
          Modern laptop CPUs perform over two billion operations per second.
          None of your applications have any excuse for ever lagging. So{" "}
          {IDE_NAME} never does.
        </p>
      </Section>

      <Section bearIndex={1}>
        <div className="text-2xl mb-4">
          Complete Vim keybindings, out of the box
        </div>
        <p>
          Our Vim keybindings aren't a plugin. They're built into the core of
          the application and designed to work seamlessly with everything else.
        </p>
        <p>
          We're big Vim users ourselves, and designed {IDE_NAME} with
          first-class Vim support from the beginning. It feels like real Vim.
        </p>
      </Section>

      <Section bearIndex={2}>
        <div className="text-2xl mb-4">No language server overhead</div>
        <p>
          {IDE_NAME}'s intellisense is hand-written, extensively tested, and
          designed for speed and reliability.
        </p>
        <p>
          We don't do things like send a JSON packet over a socket every
          keystroke. And you'll never need to restart your IDE when gopls
          crashes.
        </p>
      </Section>

      <Section bearIndex={3}>
        <div className="text-2xl mb-4">A full-fledged IDE, as fast as Vim.</div>
        <p>
          {IDE_NAME} includes everything you need to effectively develop Go
          applications: autocomplete, parameter hints, auto format, build,
          integrated debugger &mdash; all at lightning speed.
        </p>
      </Section>

      <div className="flex space-x-8">
        <div className="w-1/4">
          {["Autocomplete", "Debugging", "Some other demo", "Another demo"].map(
            (name, idx) => (
              <div
                key={name}
                className={`hover:bg-gray-100 border-b p-4 cursor-pointer ${
                  idx === 0 ? "border-t" : ""
                }`}
              >
                {name}
              </div>
            )
          )}
        </div>
        <div className="w-3/4">
          <iframe
            width="100%"
            height="420"
            src="https://www.youtube.com/embed/dQw4w9WgXcQ"
            title="YouTube video player"
            frameborder="0"
            allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
            allowfullscreen
          ></iframe>
        </div>
      </div>
    </div>
  );
}

function Pricing() {
  return (
    <div>
      <div className="text-center my-36">
        <div className="text-3xl mb-5">
          {IDE_NAME} is{" "}
          <b className="decoration-clone bg-clip-text bg-gradient-to-b from-blue-400 to-blue-700 text-transparent">
            $10/month
          </b>{" "}
          per seat.
        </div>
        <div className="my-10">
          <Link className="main-button text-lg" to="/beta">
            Join Beta
          </Link>
        </div>
        <p className="text-sm text-gray-500">
          If you're buying licenses for your team,{" "}
          <a className="underline" href="mailto:enterprise@codeperfect95.com">
            ask
          </a>{" "}
          for a bulk discount.
        </p>
      </div>
    </div>
  );
}

function Beta() {
  const [disabled, setDisabled] = React.useState(false);

  const onBuy = () => {
    setDisabled(true);
  };

  return (
    <div className="max-w-3xl my-28 mx-auto">
      <p className="text-xl font-medium">Before you sign up...</p>
      <p>
        {IDE_NAME} costs <b>$10/month</b>.
      </p>
      <p>
        It's in very early beta. We're still building out large chunks of
        functionality. We're releasing it now because we use it every day
        ourselves, and realized we'd crossed the threshold of getting more than
        $10/mo of utility from it. That said, it currently has several
        limitations. Please read them carefully to make sure we're compatible
        with your needs.
      </p>
      <ul className="thick-list">
        <li>Only Windows is supported.</li>
        <li>Only Go 1.16+ is supported.</li>
        <li>
          Your project must be module-aware, and consist of a single module,
          located at your project's root folder.
        </li>
        <li>
          You can only edit using Vim keybindings. Arrow keys and clicking (in
          the editor) don't work. We're big Vim users.
        </li>
        <li>No Find (and Replace) Everywhere.</li>
        <li>No built-in terminal.</li>
        <li>No WSL support.</li>
        <li>
          No Vim commands. Importantly, this means no{" "}
          <code>:%s/find/replace</code>. (Normal <code>/search</code> works.)
        </li>
      </ul>
      <p>
        Despite these limitations, everyone on the development team already uses{" "}
        {IDE_NAME} for day-to-day Go programming. We still need to escape-hatch
        into other tools every now and then, and you will too. As we approach
        feature completeness, your escape-hatch frequency will go down to zero.
      </p>
      <p>The following features are working:</p>
      <ul className="thick-list">
        <li>Automatically index your code and its dependencies in real-time</li>
        <li>Autocomplete (member + keyword)</li>
        <li>Parameter hints (show function signature on call)</li>
        <li>Jump to definition</li>
        <li>Auto format on save (with goimports)</li>
        <li>Vim keybindings</li>
        <li>
          Debugging with call stack, breakpoints, local variables, and watches
        </li>
        <li>Building, navigating between errors</li>
        <li>Fuzzy file picker</li>
        <li>Debug individual tests</li>
      </ul>
      <p>
        If your situation is compatible with this, you can start using a power
        tool today that allows you to develop Go programs at lightning speed.
        We're also developing new features rapidly &mdash; see what's on the{" "}
        <Link to="/roadmap">roadmap</Link>.
      </p>
      <p className="mt-8">
        <button
          onClick={onBuy}
          className="main-button text-lg px-8 py-4 disabled:opacity-50"
          disabled={disabled}
        >
          Buy {IDE_NAME}
        </button>
      </p>
    </div>
  );
}

function Roadmap() {
  return (
    <div>
      <h2 className="text-lg font-medium">Roadmap</h2>
      <ul className="thick-list">
        <li>
          macOS and Linux support. The bulk of our code is cross-platform.
          There's about 200 lines of OS-specific code that needs to be
          implemented (and tested) for each new OS.
        </li>
        <li>Make the editor usable for non-Vim users.</li>
        <li>Git integration.</li>
        <li>A more sophisticated file browser.</li>
        <li>WSL support.</li>
        <li>
          Go to symbol &mdash; like fuzzy file picker, but you're picking names
          of declared symbols instead of files.
        </li>
        <li>Improved details in autocomplete menu.</li>
        <li>(add more stuff here)</li>
      </ul>
    </div>
  );
}

/*
function About() {
  return (
    <div>
      <h2 className="text-lg font-medium">About</h2>
      <p>{IDE_NAME} is created by Brandon Hsiao.</p>
    </div>
  );
}
*/

function PaymentCanceled() {
  return <div>Your payment was canceled.</div>;
}

function PaymentSuccess() {
  return <div>Thanks! Please check your email for details.</div>;
}

function App() {
  return (
    <Router>
      <Helmet>
        <meta charSet="utf-8" />
        <title>{IDE_NAME}</title>
      </Helmet>

      <div className="max-w-5xl mx-auto mt-8 leading-relaxed text-gray-800">
        <div className="flex justify-between mb-8 items-center border-b border-gray-200 pb-4">
          <div>
            <div>
              <Link to="/" className="text-lg font-bold text-black">
                {IDE_NAME}
              </Link>
            </div>
          </div>
          <div className="flex items-baseline">
            <Link className="mr-5" to="/docs">
              Documentation
            </Link>
            <Link className="mr-5" to="/pricing">
              Pricing
            </Link>

            <Link className="main-button" to="/beta">
              Join Beta
            </Link>
          </div>
        </div>
        <Switch>
          <Route path="/docs">blah blah</Route>
          <Route path="/pricing">
            <Pricing />
          </Route>
          <Route path="/beta">
            <Beta />
          </Route>
          <Route path="/payment-canceled">
            <PaymentCanceled />
          </Route>
          <Route path="/payment-success">
            <PaymentSuccess />
          </Route>
          <Route path="/roadmap">
            <Roadmap />
          </Route>
          <Route exact path="/">
            <Home />
          </Route>
        </Switch>
        <div className="flex justify-between mt-48 mb-12 text-sm pt-4 border-t border-gray-100">
          <div class="text-gray-400">
            &copy; {CURRENT_YEAR} {IDE_NAME}
          </div>
          <div className="flex space-x-4">
            <a
              className="text-gray-400"
              href="mailto:support@codeperfect95.com"
            >
              Contact
            </a>
          </div>
        </div>
      </div>
    </Router>
  );
}

export default App;
