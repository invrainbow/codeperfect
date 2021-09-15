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

// static assets
import ideScreenshotImage from "./ide.png";
import gif60fps from "./60fps.gif";
import gifVim from "./vim.gif";
import pngIntellisense from "./intellisense.png";

// constants
const NAME = "CodePerfect 95";
const NAME_SHORT = "CodePerfect";
const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_YEAR = new Date().getFullYear();
const BETA_SIGNUP_LINK = "https://airtable.com/shraN38Z2jqQJVqbk";

function WallOfText({ title, children }) {
  return (
    <div className="wall-of-text bg-gray-100 py-5 my-6 md:p-12 rounded-md leading-normal">
      <div className="md:max-w-2xl md:mx-auto">
        {title && <Title>{title}</Title>}
        {children}
      </div>
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-xl font-bold text-gray-700" {...props}>
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
  "Instant Startup",
  "Syntax Highlighting",
]);

function Home() {
  return (
    <div>
      <div className="mt-8 mb-24 sm:my-24 md:my-32 md:mt-24">
        <div className="leading-tight text-center mb-2 text-4xl md:text-5xl font-bold text-black">
          The Fastest IDE for Go
        </div>
        <div className="mb-6 md:mb-20 text-lg text-gray-400 text-center">
          With Windows and macOS support
        </div>
        <img
          alt=""
          className="nice-image max-w-auto"
          src={ideScreenshotImage}
        />
      </div>

      <Section className="md:block lg:flex">
        <div className="leading-relaxed mb-4 md:flex md:flex-row lg:block md:gap-8 md:mb-4 lg:w-1/3">
          <h2 className="text-2xl mb-4 text-gray-700 md:w-4/12 lg:w-full font-medium">
            No Electron. No JavaScript. No garbage collection.
          </h2>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            {NAME_SHORT} is written in C++ and designed to run at 144 FPS.
            Every keystroke responds instantly.
          </p>
        </div>
        <NiceImage src={gif60fps} className="lg:w-2/3" />
      </Section>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed md:w-5/12">
          <h2 className="text-2xl mb-4 text-gray-700 font-medium">
            A batteries-included IDE, as fast as Vim.
          </h2>
          <p>
            Today, IDEs are powerful but slow, while Vim is fast but limited.{" "}
            {NAME_SHORT} brings the best of both worlds: everything you need
            to effectively develop in Go, at lightning speed.
          </p>
          <p>
            No more waiting on GoLand. No more hacking Vim plugins together.
          </p>
        </div>
        <div className="md:w-7/12 mt-8 md:mt-0 grid grid-cols-3 sm:grid-cols-4 md:grid-cols-4 lg:grid-cols-4 gap-3 md:gap-6 select-none">
          {IDE_FEATURES.map((feature, i) => (
            <div
              key={i}
              className="text-sm md:text-md leading-tight font-semibold h-16 md:h-20 lg:h-20 rounded-lg text-gray-500 hover:text-gray-600 text-center flex justify-center items-center p-4 transform hover:scale-110 transition-all shadow-sm"
              style={{ background: "#eee" }}
            >
              <span>{feature}</span>
            </div>
          ))}
        </div>
      </Section>

      <div className="block md:hidden">
        <Section>
          <div className="leading-relaxed md:flex md:flex-row md:gap-8 md:mb-12">
            <div className="text-2xl mb-4 text-gray-700 md:w-1/3 font-medium">
              Tools that understand Go as a first language.
            </div>
            <p className="md:w-1/3 md:mt-0">
              {NAME_SHORT}'s intellisense is hand-written, extensively
              tested, and designed for speed and reliability.
            </p>
          </div>
        </Section>
      </div>

      <div className="hidden md:block relative rounded-md overflow-hidden border-gray-500 border mb-24 md:mb-32">
        <div
          className="leading-relaxed md:gap-8 w-5/12 z-20 relative p-8 drop-shadow rounded-md my-12 mx-auto"
          style={{ background: "rgba(0, 0, 0, 0.75)" }}
        >
          <div className="text-2xl mb-4 text-white font-medium">
            Tools that understand Go as a first language.
          </div>
          <p className="text-white">
            {NAME_SHORT}'s intellisense is hand-written, extensively tested,
            and designed for speed and reliability.
          </p>
        </div>
        <img
          src={pngIntellisense}
          alt=""
          className="absolute top-0 left-0 z-10 w-full opacity-50"
        />
      </div>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed mb-4 md:mb-0 md:w-5/12">
          <div className="text-2xl mb-4 text-gray-700 font-medium">
            Complete Vim keybindings, out of the box.
          </div>
          <p>
            Our Vim keybindings aren't a plugin added as an afterthought.
            They're built into the core of the application and designed to work
            seamlessly with everything else.
          </p>
        </div>
        <NiceImage src={gifVim} className="md:w-7/12" />
      </Section>
    </div>
  );
}

function PricingBox({ title, price, subprice, children }) {
  return (
    <div className="w-auto md:w-1/3 rounded-lg text-center overflow-hidden bg-white border">
      <div className="p-6">
        <h1 className="font-bold text-gray-700 text-lg mb-2">{title}</h1>
        <div className="font-bold text-black text-2xl">{price}</div>
        <div className="mt-1">
          <span className="rounded-full bg-gray-100 text-gray-600 leading-4 py-0.5 px-2 inline-block font-medium text-sm">
            {subprice}
          </span>
        </div>
      </div>
      <div className="border-t p-6">
        <div className="text-gray-500 text-left">{children}</div>
      </div>
    </div>
  );
}

function Pricing() {
  return (
    <div className="pricing my-16">
      <h1 className="text-center text-black font-bold text-4xl mb-12">
        Pricing
      </h1>
      <div className="">
        <div className="max-w-5xl flex flex-col md:flex-row space-between space-y-8 space-x-0 md:space-y-0 md:space-x-12 mx-auto">
          <PricingBox
            title="Individual Plan"
            tier="individual"
            price="$5/mo"
            subprice="or $50/year"
          >
            Commercial use is allowed, but a company cannot pay for this, and
            you cannot expense the purchase.
          </PricingBox>

          <PricingBox
            title="Company Plan"
            price="$10/user/mo"
            tier="company"
            subprice="or $100/user/year"
          >
            For companies buying licenses for its employees. Most users choose
            this plan in order to expense the purchase.
          </PricingBox>

          <PricingBox
            title="Premium Plan"
            tier="premium"
            price="$20/user/month"
            subprice="or $200/user/year"
          >
            Same as company plan, but also comes with priority support.{" "}
            <a href="mailto:support@codeperfect95.com">Contact us</a> for
            details.
          </PricingBox>
        </div>
      </div>
      <div className="mt-16 text-center">
        <p className="text-xl mb-0">Ready to get started?</p>
        <p>
          <a
            target="_blank"
            rel="noreferrer"
            className="main-button font-bold text-xl px-6 py-3 whitespace-nowrap"
            href={BETA_SIGNUP_LINK}
          >
            Request Access
          </a>
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
        When you fill out the Join Beta form, we collect your name and email. We
        use this to send you updates about new product features.
      </p>
      <p>
        When you sign up, we collect your name, email, and credit card
        information. We use this information to bill you and send you emails
        with updates about your payment status (for example, if your card
        fails).
      </p>
      <p>
        The IDE contacts the server to authenticate your license key and to
        install automatic updates. This exposes your IP address to us. We won't
        share it with anyone, unless ordered to by law.
      </p>
    </WallOfText>
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
        <title>{NAME}</title>
      </Helmet>

      <div className="p-6 md:p-12 text-gray-500 w-full lg:max-w-screen-xl lg:mx-auto">
        <div className="mx-auto pb-4 flex justify-between items-center">
          <Link
            to="/"
            className="font-bold text-black no-underline whitespace-nowrap"
          >
            {NAME}
          </Link>
          <div className="flex items-baseline space-x-6">
            <a
              className="no-underline font-semibold text-gray-600 hidden sm:inline-block"
              href="https://docs.codeperfect95.com"
              target="_blank"
              rel="noreferrer"
            >
              Docs
            </a>

            <Link
              className="no-underline font-semibold text-gray-600 hidden sm:inline-block"
              to="/pricing"
            >
              Pricing
            </Link>

            <a
              target="_blank"
              rel="noreferrer"
              className="main-button whitespace-nowrap"
              href={BETA_SIGNUP_LINK}
            >
              Join Beta
            </a>
          </div>
        </div>
        <div className="">
          <Switch>
            <Route path="/pricing">
              <Pricing />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Privacy />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
          </Switch>
        </div>
        <div className="pt-4">
          <div className="lg:max-w-screen-xl lg:mx-auto flex flex-col sm:flex-row justify-between">
            <div className="text-gray-500">
              &copy; {CURRENT_YEAR} {NAME}
            </div>
            <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-12 mt-2 sm:mt-0">
              <div className="text-left">
                <div>
                  <a
                    target="_blank"
                    rel="noreferrer"
                    className="text-gray-500 no-underline"
                    href={BETA_SIGNUP_LINK}
                  >
                    Join Beta
                  </a>
                </div>
                <div>
                  <a
                    target="_blank"
                    rel="noreferrer"
                    className="text-gray-500 no-underline"
                    href="https://docs.codeperfect95.com"
                  >
                    Docs
                  </a>
                </div>
                <div>
                  <Link to="/pricing" className="text-gray-500 no-underline">
                    Pricing
                  </Link>
                </div>
              </div>
              <div className="text-left">
                <div>
                  <Link to="/terms" className="text-gray-500 no-underline">
                    Terms of Service
                  </Link>
                </div>
                <div>
                  <Link className="text-gray-500 no-underline" to="/privacy">
                    Privacy Policy
                  </Link>
                </div>
                <div>
                  <a
                    className="text-gray-500 no-underline"
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
