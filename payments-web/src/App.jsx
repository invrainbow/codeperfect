/* global Stripe */

import React from "react";
import { BrowserRouter as Router, Switch, Route, Link } from "react-router-dom";
import { Helmet } from "react-helmet";
import cx from "classnames";
import _ from "lodash";

// images
import ideScreenshotImage from "./ide.png";
import gif60fps from "./60fps.gif";
import gifVim from "./vim.gif";
import pngIntellisense from "./intellisense.png";

const IDE_NAME = "CodePerfect 95";
const DOMAIN = "codeperfect95.com";
const SUPPORT_EMAIL = `support@${DOMAIN}`;
const ENTERPRISE_EMAIL = `enterprise@${DOMAIN}`;

const API_BASE = "http://localhost:8080";

const CURRENT_YEAR = new Date().getFullYear();

// const STRIPE_PRICE_ID = "price_1IrHFLBpL0Zd3zdOjGoBlmZF";
// const STRIPE_PUB_KEY =
//   "pk_test_51IqLcpBpL0Zd3zdOPzAZQbYjpmD47ethoqtcFGwiJBLdovijF8G0hBTA8FylfnRnQ8aXoPVC2DmNMHpndiV1YtJr00UU0XCWnt";

// const stripe = Stripe(STRIPE_PUB_KEY);

function WallOfText({ title, children }) {
  return (
    <div className="border border-gray-700 p-12 rounded-sm my-8">
      <div className="max-w-3xl mx-auto leading-relaxed text-gray-200">
        <Title>{title}</Title>
        {children}
      </div>
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-lg font-semibold" {...props}>
      {children}
    </h2>
  );
}

function NiceImage({ className, ...props }) {
  return (
    <div>
      <img
        alt=""
        className={cx("overflow-hidden nice-image max-w-none", className)}
        {...props}
      />
    </div>
  );
}

function Section({ className, ...props }) {
  return (
    <div
      className={cx("space-x-14 flex mb-32 items-center", className)}
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
  "Debug Tests",
  "Instant Startup",
  "Syntax Highlighting",
]);

function Home() {
  return (
    <div>
      <div className="my-28">
        <div className="text-center mb-12 text-5xl font-light text-white">
          A Blazing Fast IDE for Go
        </div>
        <img
          alt=""
          className="nice-image max-w-auto"
          src={ideScreenshotImage}
        />
      </div>

      <Section>
        <div className="leading-relaxed">
          <div className="text-2xl mb-4 text-gray-200">
            No Electron. No JavaScript. No garbage collection.
          </div>
          <p>
            {IDE_NAME} is written in C++ and designed to run at 144 FPS. Every
            keystroke responds instantly.
          </p>
          <p>
            Modern laptop CPUs perform over two billion operations per second.
            None of your apps should ever lag. {IDE_NAME} never does.
          </p>
        </div>
        <NiceImage src={gif60fps} className="h-80" />
      </Section>

      <Section>
        <div className="flex flex-wrap gap-4 w-3/5 select-none">
          {IDE_FEATURES.map((feature, i) => (
            <div
              key={i}
              className="font-semibold w-28 h-24 rounded-lg text-gray-400 text-center flex justify-center items-center text-sm p-4 transform hover:scale-110 transition-all"
              style={{ background: "#282828" }}
            >
              <span>{feature}</span>
            </div>
          ))}
        </div>
        <div className="leading-relaxed flex-1">
          <div className="text-2xl mb-4 text-gray-200">
            A full-fledged IDE, as fast as Vim.
          </div>
          <p>
            Today, IDEs are powerful but slow, while Vim is fast but limited.
          </p>
          <p>
            {IDE_NAME} brings the best of both worlds: everything you need to
            effectively develop in Go, at lightning speed.
          </p>
          <p>
            No more waiting on GoLand's garbage collector. No more hacking
            plugins together in ~/.vimrc.
          </p>
        </div>
      </Section>

      <Section>
        <div className="leading-relaxed">
          <div className="text-2xl mb-4 text-gray-200">
            No more language server overhead.
          </div>
          <p>
            {IDE_NAME}'s intellisense is hand-written, extensively tested, and
            designed for speed and reliability.
          </p>
          <p>
            We'll never do things like send a JSON packet over a socket every
            keystroke. And you'll never need to restart your IDE because gopls
            crashed.
          </p>
        </div>
        <NiceImage src={pngIntellisense} className="h-auto" />
      </Section>

      <Section>
        <NiceImage src={gifVim} className="h-96" />
        <div className="leading-relaxed">
          <div className="text-2xl mb-4 text-gray-200">
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
      </Section>

      <p className="text-center mb-8">
        Watch a demo of {IDE_NAME} being used to solve{" "}
        <a target="_blank" rel="noreferrer" href="https://cryptopals.com">
          Cryptopals
        </a>
        .
      </p>

      <div className="mx-auto" style={{ maxWidth: "600px" }}>
        <div
          className="mb-32 relative w-full h-0"
          style={{ paddingBottom: "56.25%" }}
        >
          <iframe
            width="100%"
            className="absolute top-0 left-0 w-full h-full"
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

  // const onBuy = async () => {
  //   setDisabled(true);
  //   try {
  //     const data = await talkToServer("checkout", {
  //       price_id: STRIPE_PRICE_ID,
  //     });
  //     stripe.redirectToCheckout({ sessionId: data.session_id });
  //   } catch (err) {
  //     setDisabled(false);
  //     alert(err);
  //   }
  // };

  return (
    <div className="border-t border-b py-12" style={{ borderColor: "#282828" }}>
      <div className="text-center">
        <div className="text-2xl mb-0">
          {IDE_NAME} is <b className="text-white">$10/month</b> per seat.
        </div>
        {/* <p className="mt-3 text-sm text-gray-400">
          If you're buying licenses for your team,{" "}
          <a
            className="underline text-gray-300"
            href={`mailto:${ENTERPRISE_EMAIL}`}
          >
            ask
          </a>{" "}
          for a bulk discount.
        </p> */}
      </div>

      <div className="max-w-2xl mx-auto mt-8">
        <Title>Before you sign up...</Title>
        <p>
          {IDE_NAME} is still in early beta. We're releasing it now because we
          use it every day ourselves, and realized we were getting more than $10
          of monthly utility from it. That said, there are currently several
          large limitations:
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
            Editing via Vim keys only. Arrow keys, clicking, and scrolling don't
            work in the editor yet.
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
            No Vim commands. Importantly, no <code>:%s/search/replace</code>. (
            <code>/search</code> works.)
          </li>
        </ul>
        <p>
          Nevertheless, {IDE_NAME} is ready for day-to-day, bread-and-butter Go
          programming. Everyone on the dev team uses it. The following are
          working:
        </p>
        <ul className="thick-list">
          <li>
            Automatically index your code and its dependencies in real-time
          </li>
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
          <li>Extremely optimized core</li>
        </ul>
        <p>
          If this sounds compatible with your setup, {IDE_NAME} will allow you
          to start developing Go programs at lightning speed today. We're also{" "}
          <Link to="/roadmap">steadily developing new features</Link>.
        </p>
        <p className="my-8 text-center">
          {/* <button
            onClick={onBuy}
            className="main-button text-lg px-8 py-4"
            disabled={disabled}

            Sign up
          </button> */}
          <a
            className="main-button text-lg px-8 py-4"
            href="https://967gb74hmbf.typeform.com/to/nVtqlzdj"
            target="_blank"
            rel="noreferrer"
          >
            Request Access
          </a>
        </p>
        <p>
          If you're interested in {IDE_NAME} but it's not a fit right now, you
          can also subscribe to email updates below.
        </p>
        <form
          action="https://gmail.us6.list-manage.com/subscribe/post?u=530176c3897958e56302043ed&amp;id=cb045d5e14"
          className="mt-8 text-center p-6 rounded-md shadow-md"
          style={{
            border: "solid 1px #444",
          }}
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
            className="py-1.5 px-2.5 rounded-md text-sm mr-2"
            style={{
              background: "#222",
              border: "solid 1px #555",
            }}
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
          <button
            className="main-button from-gray-200 to-gray-300 text-gray-600"
            type="submit"
          >
            Subscribe
          </button>
          <p className="text-xs text-gray-400 mt-4">
            (We'll only send you product updates; we won't spam you or share
            your email.)
          </p>
        </form>
      </div>
    </div>
  );
}

function Roadmap() {
  return (
    <WallOfText title="Roadmap">
      <p>Here are things we're looking to add in the immediate future.</p>
      <ul className="roadmap-list text-gray-400">
        <li>macOS and Linux support.</li>
        <li>Make the editor usable for non-Vim users.</li>
        <li>Git integration.</li>
        <li>A more sophisticated file browser.</li>
        <li>WSL support.</li>
        <li>
          Go to symbol &mdash; like fuzzy file picker, but for names of declared
          identifiers instead of files.
        </li>
        <li>Improved details in autocomplete menu.</li>
        <li>Find all usages.</li>
        <li>Automatic refactoring.</li>
        <li>Support symlinks.</li>
        <li>Support large repositories.</li>
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
    <WallOfText title="Your payment was canceled.">
      <p>
        If you didn't mean to cancel it, you can{" "}
        <Link to="/beta">try again</Link>.
      </p>
      <p>
        If you believe the payment went through and you were charged, please{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>email us</a>.
      </p>
      <p>
        Otherwise, <Link to="/">click here</Link> to return to the main page.
      </p>
    </WallOfText>
  );
}

function PaymentSuccess() {
  return (
    <WallOfText title="Your payment went through!">
      <p>Please check your email for the download link and your license key.</p>
      <p>
        If the email doesn't come, please check your spam folder and wait a few
        minutes. If it still doesn't come, please do not purchase a second time
        &mdash; two subscriptions will be created. Instead,{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>email us</a>.
      </p>
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

      <div className="py-6 leading-relaxed text-gray-400">
        <div className="px-4 max-w-6xl mx-auto flex justify-between items-center">
          <Link to="/" className="text-lg font-bold text-white no-underline">
            {IDE_NAME}
          </Link>
          <div className="flex items-baseline space-x-6">
            <Link className="main-button" to="/beta">
              Request Access
            </Link>
          </div>
        </div>
        <div className="my-4 px-4 max-w-6xl mx-auto">
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
        <div className="text-sm">
          <div className="px-4 max-w-6xl mx-auto flex justify-between">
            <div className="text-gray-400">
              &copy; {CURRENT_YEAR} {IDE_NAME}
            </div>
            <div className="flex space-x-6">
              <Link to="/roadmap" className="text-gray-400 no-underline">
                Roadmap
              </Link>
              <Link to="/terms" className="text-gray-400 no-underline">
                Terms of Service
              </Link>
              <Link className="text-gray-400 no-underline" to="/privacy">
                Privacy Policy
              </Link>
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
    </Router>
  );
}

export default App;
