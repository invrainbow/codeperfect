/* global Stripe */

import React from "react";
import { BrowserRouter as Router, Switch, Route, Link } from "react-router-dom";
import ideScreenshotImage from "./ide.png";
import { Helmet } from "react-helmet";

import bear1Image from "./bear1.jpg";
import bear2Image from "./bear2.jpg";
import bear3Image from "./bear3.jpg";
import bear4Image from "./bear4.jpg";

const IDE_NAME = "CodePerfect 95";
const API_BASE = "http://localhost:8080";
const CURRENT_YEAR = new Date().getFullYear();
const STRIPE_PRICE_ID = "price_1IrHFLBpL0Zd3zdOjGoBlmZF";
const STRIPE_PUB_KEY =
  "pk_test_51IqLcpBpL0Zd3zdOPzAZQbYjpmD47ethoqtcFGwiJBLdovijF8G0hBTA8FylfnRnQ8aXoPVC2DmNMHpndiV1YtJr00UU0XCWnt";

const stripe = Stripe(STRIPE_PUB_KEY);

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

function Title(props) {
  return <h2 className="text-lg font-semibold" {...props} />;
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

      <div className="space-x-8 flex mb-32">
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

  const onBuy = async () => {
    setDisabled(true);
    try {
      const resp = await fetch(`${API_BASE}/checkout`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ price_id: STRIPE_PRICE_ID }),
      });
      const data = await resp.json();
      stripe.redirectToCheckout({ sessionId: data.session_id });
    } catch (err) {
      setDisabled(false);
      alert(err);
    }
  };

  return (
    <div className="max-w-2xl my-12">
      <p className="text-xl font-semibold">Before you sign up...</p>
      <p>{IDE_NAME} costs $10/month.</p>
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
          located at your project's root folder. More specifically,{" "}
          <code>go list -mod=mod -m all</code> must work from your root folder.
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
        Despite these limitations, {IDE_NAME} is ready for day-to-day,
        bread-and-butter Go programming. Everyone on the development team uses
        it. You still need to escape-hatch into other tools every now and then,
        but as we approach feature completeness, this will happen less and less.
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
        If this sounds compatible with your setup, {IDE_NAME} will allow you to
        start developing Go programs at lightning speed today. We're also
        developing new features rapidly &mdash; check out the{" "}
        <Link to="/roadmap">roadmap</Link>.
      </p>
      <p className="my-8">
        <button
          onClick={onBuy}
          className="main-button text-lg px-8 py-4 disabled:opacity-50"
          disabled={disabled}
        >
          Buy {IDE_NAME}
        </button>
      </p>
      <p>
        If you're interested in {IDE_NAME} but it's not a fit right now, you can
        also subscribe to email updates below.
      </p>
      <form
        action="https://gmail.us6.list-manage.com/subscribe/post?u=530176c3897958e56302043ed&amp;id=cb045d5e14"
        className="mt-8"
        method="post"
        name="mc-embedded-subscribe-form"
        target="_blank"
        novalidate
      >
        <input
          type="email"
          defaultValue=""
          name="EMAIL"
          placeholder="Email address"
          required
          className="border border-black-400 py-1.5 px-2.5 rounded-md text-sm mr-2"
        />
        <div
          style={{ position: "absolute", left: "-5000px" }}
          aria-hidden="true"
        >
          <input
            type="text"
            name="b_530176c3897958e56302043ed_cb045d5e14"
            tabindex="-1"
            value=""
          />
        </div>
        <input
          className="main-button from-gray-200 to-gray-300 text-gray-600"
          role="button"
          type="submit"
          value="Subscribe"
          name="subscribe"
        />
        <p className="text-xs text-gray-400 mt-1">
          (We'll only send you product updates; we won't spam you or share your
          email.)
        </p>
      </form>
    </div>
  );
}

function Roadmap() {
  return (
    <div>
      <Title>Roadmap</Title>
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
      <Title>About</Title>
      <p>{IDE_NAME} is created by Brandon Hsiao.</p>
    </div>
  );
}
*/

function PaymentCanceled() {
  return (
    <div>
      <Title>Your payment was canceled.</Title>
      <p>
        If you didn't mean to cancel it, you can{" "}
        <Link to="/beta">try again</Link>.
      </p>
      <p>
        If you believe the payment went through and you were charged, please{" "}
        <a href="mailto:support@codeperfect95.com">email us</a>.
      </p>
      <p>
        Otherwise, <Link to="/">click here</Link> to return to the main page.
      </p>
    </div>
  );
}

function PaymentSuccess() {
  return (
    <div>
      <Title>Your payment went through!</Title>
      <p>Please check your email for the download link and your license key.</p>
      <p>
        If the email doesn't come, please check your spam folder and wait a few
        minutes. If it still doesn't come, please do not purchase a second time
        &mdash; two subscriptions will be created. Instead,{" "}
        <a href="mailto:support@codeperfect95.com">email us</a>.
      </p>
    </div>
  );
}

function Docs() {
  return (
    <div className="max-w-2xl my-12">
      <Title>Docs</Title>
      <p>Blah blah blah.</p>
    </div>
  );
}

function Philosophy() {
  return (
    <div className="max-w-2xl my-12">
      <Title>Philosophy</Title>
      <p>Blah blah blah.</p>
    </div>
  );
}

function App() {
  return (
    <Router>
      <Helmet>
        <meta charSet="utf-8" />
        <title>{IDE_NAME}</title>
      </Helmet>

      <div className="py-6 leading-relaxed text-gray-800">
        <div className="px-4 max-w-6xl mx-auto flex justify-between mb-8 items-center pb-4">
          <Link to="/" className="text-lg font-bold text-black">
            {IDE_NAME}
          </Link>
          <div className="flex items-baseline">
            <Link className="mr-5" to="/philosophy">
              Philosophy
            </Link>
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
        <div className="px-4 max-w-6xl mx-auto">
          <Switch>
            <Route path="/docs">
              <Docs />
            </Route>
            <Route path="/philosophy">
              <Philosophy />
            </Route>
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
        </div>
        <div className="mt-8 text-sm pt-4">
          <div className="px-4 max-w-6xl mx-auto flex justify-between">
            <div className="text-gray-400">
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
      </div>
    </Router>
  );
}

export default App;
