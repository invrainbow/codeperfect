/* global Stripe */

import React from "react";
import { BrowserRouter as Router, Switch, Route, Link } from "react-router-dom";
import ideScreenshotImage from "./ide.png";
import { Helmet } from "react-helmet";
import cx from "classnames";

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

function WallOfText({ title, children }) {
  return (
    <div className="bg-gray-100 p-8 rounded-sm">
      <div className="max-w-2xl bg-white p-8 mx-auto rounded-sm shadow-sm text-sm">
        <Title>{title}</Title>
        {children}
      </div>
    </div>
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
        <div className="text-2xl mb-4">A full-fledged IDE, as fast as Vim.</div>
        <p>Today, IDEs are powerful but slow, Vim fast but limited.</p>
        <p>
          {IDE_NAME} brings the best of both worlds: everything you need to
          effectively develop Go applications &mdash; jump to definition,
          autocomplete, parameter hints, auto format, build, integrated
          debugger, and more &mdash; at lightning speed.
        </p>
        <p>
          No more waiting on Goland's garbage collector. No more hacking plugins
          together in ~/.vimrc.
        </p>
      </Section>

      <Section bearIndex={2}>
        <div className="text-2xl mb-4">No more language server overhead.</div>
        <p>
          {IDE_NAME}'s intellisense is hand-written, extensively tested, and
          designed for speed and reliability.
        </p>
        <p>
          We'll never do things like send a JSON packet over a socket every
          keystroke. And you'll never need to restart your IDE because gopls
          crashed.
        </p>
      </Section>

      <Section bearIndex={3}>
        <div className="text-2xl mb-4">
          Feature-complete Vim keybindings, out of the box.
        </div>
        <p>
          Our Vim keybindings aren't a plugin added as an afterthought. They're
          built into the core of the application and designed to work seamlessly
          with everything else.
        </p>
        <p>
          We're big Vim users ourselves, and designed {IDE_NAME} with
          first-class Vim support from the beginning. It basically feels like
          real Vim.
        </p>
      </Section>

      <Section bearIndex={4}>
        <div className="text-2xl mb-4">Designed to perfection.</div>
        <p>
          Programs sometimes seem written by people who don't use it themselves.
          "How did they not catch this bug that happens when I do something
          extremely basic?"
        </p>
        <p>
          We use {IDE_NAME} ourselves every single day, so we built it to work
          seamlessly, down to every micro-interaction.
        </p>
      </Section>

      <div className="space-x-8 flex mb-32">
        <div className="w-1/4">
          {["Autocomplete", "Debugging", "Some other demo", "Another demo"].map(
            (name, idx) => (
              <div
                key={name}
                className={cx(
                  "rounded-md",
                  "py-2",
                  "px-4",
                  "select-none",
                  "cursor-pointer",
                  idx === 0 && "bg-gray-200",
                  idx !== 0 && "hover:bg-gray-100"
                )}
                style={{ marginBottom: "1px" }}
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
          <Link className="main-button text-xl py-3 px-6 rounded-lg" to="/beta">
            Join Beta
          </Link>
        </div>
        <p className="text-s">
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

// I'm just going to use POST and form data for everything. Do not talk to me
// about REST or any stupid shit like that or I'll fire you.
async function talkToServer(endpoint, params) {
  const makeFormData = (obj) => {
    const ret = new URLSearchParams();
    Object.keys(obj).forEach((key) => {
      ret.append(key, obj[key]);
    });
    return ret;
  };
  const resp = await fetch(`${API_BASE}/${endpoint}`, {
    method: "POST",
    body: makeFormData(params),
  });
  return await resp.json();
}

function Portal() {
  const [disabled, setDisabled] = React.useState(false);
  const [licenseKey, setLicenseKey] = React.useState(
    "ba26c910-e37e71e3-5e07eaf4-4b844592"
  );

  const onSubmit = async (e) => {
    e.preventDefault();
    setDisabled(true);
    try {
      const data = await talkToServer("portal", {
        license_key: licenseKey,
      });

      if (!data.portal_url) {
        alert("Invalid license key.");
        return;
      }

      window.location.href = data.portal_url;
    } finally {
      setDisabled(false);
    }
  };

  return (
    <WallOfText title="Portal Login">
      <p>
        Please enter your license key to sign in to the portal. It should be in
        the email that sent you here.
      </p>
      <form onSubmit={onSubmit}>
        <input
          type="text"
          value={licenseKey}
          onChange={(e) => setLicenseKey(e.target.value)}
          placeholder="License key"
          className="border border-black-400 py-1.5 px-2.5 rounded-md text-sm mr-2 font-mono"
        />
        <button
          className="main-button from-gray-200 to-gray-300 text-gray-600"
          disabled={disabled}
          type="submit"
        >
          Log in to portal
        </button>
      </form>
    </WallOfText>
  );
}

// const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function Download() {
  const [disabled, setDisabled] = React.useState(false);
  const [licenseKey, setLicenseKey] = React.useState(
    "ba26c910-e37e71e3-5e07eaf4-4b844592"
  );

  const onSubmit = async (e) => {
    e.preventDefault();
    setDisabled(true);
    try {
      const data = await talkToServer("download", {
        license_key: licenseKey,
      });

      if (!data.download_link) {
        alert("Invalid license key.");
        return;
      }

      window.location.href = data.download_link;
    } finally {
      setDisabled(false);
    }
  };

  return (
    <WallOfText title="Download">
      <p>
        Please enter your license key to continue to the download. It should be
        in the email you got.
      </p>
      <form onSubmit={onSubmit}>
        <input
          type="text"
          value={licenseKey}
          onChange={(e) => setLicenseKey(e.target.value)}
          placeholder="License key"
          className="border border-black-400 py-1.5 px-2.5 rounded-md text-sm mr-2 font-mono"
        />
        <button
          className="main-button from-gray-200 to-gray-300 text-gray-600"
          type="submit"
          disabled={disabled}
        >
          Download
        </button>
      </form>
    </WallOfText>
  );
}

function Beta() {
  const [disabled, setDisabled] = React.useState(false);

  const onBuy = async () => {
    setDisabled(true);
    try {
      const data = await talkToServer("checkout", {
        price_id: STRIPE_PRICE_ID,
      });
      stripe.redirectToCheckout({ sessionId: data.session_id });
    } catch (err) {
      setDisabled(false);
      alert(err);
    }
  };

  return (
    <WallOfText title="Before you sign up...">
      <p>{IDE_NAME} costs $10/month.</p>
      <p>
        It's still in early beta. Large chunks of functionality remain to be
        built. We're releasing it now because we use it every day ourselves, and
        realized we'd crossed the threshold of getting $10+ of monthly utility
        from it. That said, it currently has several limitations. Please read
        them carefully to make sure we're compatible with your needs.
      </p>
      <ul className="thick-list">
        <li>Only Windows is supported.</li>
        <li>Only Go 1.13+ is supported.</li>
        <li>
          Your project must be module-aware, and consist of a single module,
          located at your project's root folder. More specifically,{" "}
          <code>go list -mod=mod -m all</code> must work from your root folder.
        </li>
        <li>
          Only editing via Vim keys. Arrow keys, clicking, and scrolling don't
          work in the editor yet.
        </li>
        <li>No project-wide Search/Search and Replace.</li>
        <li>No built-in terminal.</li>
        <li>No WSL support.</li>
        <li>
          No Vim commands. Importantly, this means no{" "}
          <code>:%s/find/replace</code>. (<code>/search</code> works.)
        </li>
      </ul>
      <p>
        Despite these limitations, {IDE_NAME} is ready for day-to-day,
        bread-and-butter Go programming (everyone on the dev team uses it). The
        following features are working:
      </p>
      <ul className="thick-list">
        <li>Automatically index your code and its dependencies in real-time</li>
        <li>Autocomplete (member & keyword)</li>
        <li>Parameter hints (show function signature on call)</li>
        <li>Jump to definition</li>
        <li>
          Auto format on save (with <code>goimports</code>)
        </li>
        <li>Vim keybindings</li>
        <li>
          Debugging with call stack, breakpoints, local variables, and watches
        </li>
        <li>Debug tests (package-wide &amp; individual tests)</li>
        <li>Building, navigating between errors</li>
        <li>Fuzzy file picker</li>
      </ul>
      <p>
        If this sounds compatible with your setup, {IDE_NAME} will allow you to
        start developing Go programs at lightning speed today. We're also{" "}
        <Link to="/roadmap">developing new features rapidly</Link>.
      </p>
      <p className="my-8">
        <button
          onClick={onBuy}
          className="main-button text-lg px-8 py-4"
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
    </WallOfText>
  );
}

function Roadmap() {
  return (
    <WallOfText title="Roadmap">
      <p>Here are things we're looking to add in the immediate future.</p>
      <ul className="roadmap-list text-gray-500">
        <li>
          macOS and Linux support. The bulk of our code is cross-platform.
          There's about 200 lines of OS-specific code that needs to be
          implemented (easy) and tested (hard) for each new OS.
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
        <li>Find all usages.</li>
      </ul>
    </WallOfText>
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
    <WallOfText title="Documentation">
      <p>TODO</p>
    </WallOfText>
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
            <Route path="/download">
              <Download />
            </Route>
            <Route path="/portal">
              <Portal />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Privacy />
            </Route>
            <Route path="/docs">
              <Docs />
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
              <Link to="/terms" className="text-gray-400">
                Terms of Service
              </Link>
              <Link className="text-gray-400" to="/privacy">
                Privacy Policy
              </Link>
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
