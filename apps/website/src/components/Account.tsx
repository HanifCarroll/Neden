import { useEffect, useState, type SubmitEvent } from "react"
import { createAuthClient } from "better-auth/react"
import { oauthProviderClient } from "@better-auth/oauth-provider/client"
import { Button } from "./ui/button"
import { Input } from "./ui/input"
import { Field, FieldGroup, FieldLabel, FieldDescription } from "./ui/field"
import { Alert, AlertDescription } from "./ui/alert"
import { Separator } from "./ui/separator"

interface Usage {
  subscribed: boolean
  hasBillingAccount: boolean
  periodEnd: number | null
  cancelAtPeriodEnd: boolean
  words: { used: number; limit: number }
  explanations: { used: number; limit: number }
}
type FormMode = "sign-in" | "register" | "forgot" | "reset"

export default function Account({ apiUrl }: { apiUrl: string }) {
  const [client] = useState(() =>
    createAuthClient({ baseURL: apiUrl, plugins: [oauthProviderClient()] })
  )
  const { data: session, isPending, error: sessionError } = client.useSession()
  const [mode, setMode] = useState<FormMode>(() =>
    typeof window !== "undefined" &&
    new URLSearchParams(window.location.search).has("token")
      ? "reset"
      : "sign-in"
  )
  const [message, setMessage] = useState("")
  const [busy, setBusy] = useState(false)
  const [usage, setUsage] = useState<Usage | null>(null)
  const [billingAvailable, setBillingAvailable] = useState(false)
  const [sandbox, setSandbox] = useState(true)

  useEffect(() => {
    fetch(`${apiUrl}/v1/config`)
      .then((response) => response.json())
      .then((config) => {
        setBillingAvailable(config.billingAvailable === true)
        setSandbox(config.sandbox !== false)
      })
      .catch(() =>
        setMessage(
          "Account service is temporarily unavailable. Please try again."
        )
      )
  }, [apiUrl])
  useEffect(() => {
    if (!session) return
    const controller = new AbortController()
    fetch(`${apiUrl}/v1/account`, {
      credentials: "include",
      signal: controller.signal,
    })
      .then(async (response) => {
        if (!response.ok) throw new Error()
        setUsage(await response.json())
      })
      .catch(() => {
        if (!controller.signal.aborted)
          setMessage(
            "Could not load your allowance. Please refresh to try again."
          )
      })
    return () => controller.abort()
  }, [session, apiUrl])

  async function submit(event: SubmitEvent<HTMLFormElement>) {
    event.preventDefault()
    const data = new FormData(event.currentTarget)
    const email = String(data.get("email") ?? "")
    const password = String(data.get("password") ?? "")
    setBusy(true)
    setMessage("")
    try {
      const callbackURL = `${window.location.origin}/account/`
      if (mode === "register") {
        const result = await client.signUp.email({
          name: String(data.get("name")),
          email,
          password,
          callbackURL,
        })
        if (result.error) throw new Error(result.error.message)
        setMessage("Check your email to verify your account, then sign in.")
      } else if (mode === "forgot") {
        const result = await client.requestPasswordReset({
          email,
          redirectTo: callbackURL,
        })
        if (result.error) throw new Error(result.error.message)
        setMessage(
          "If an account uses that email, a password reset link is on its way."
        )
      } else if (mode === "reset") {
        const token =
          new URLSearchParams(window.location.search).get("token") ?? ""
        const result = await client.resetPassword({
          newPassword: password,
          token,
        })
        if (result.error) throw new Error(result.error.message)
        window.history.replaceState({}, "", "/account/")
        setMode("sign-in")
        setMessage("Password updated. Sign in with your new password.")
      } else {
        const result = await client.signIn.email({
          email,
          password,
          callbackURL,
        })
        if (result.error) throw new Error(result.error.message)
      }
    } catch (error) {
      setMessage(
        error instanceof Error
          ? error.message
          : "Could not complete this request. Please try again."
      )
    } finally {
      setBusy(false)
    }
  }
  async function openBilling(path: "checkout" | "portal") {
    setBusy(true)
    setMessage("")
    try {
      const response = await fetch(`${apiUrl}/v1/billing/${path}`, {
        method: "POST",
        credentials: "include",
      })
      const result = await response.json()
      if (!response.ok)
        throw new Error(result.error?.message ?? "Billing is unavailable.")
      const destination = new URL(result.url)
      if (
        destination.protocol !== "https:" ||
        !["checkout.stripe.com", "billing.stripe.com"].includes(
          destination.hostname
        )
      )
        throw new Error("Could not open billing securely.")
      window.location.assign(destination.href)
    } catch (error) {
      setMessage(
        error instanceof Error ? error.message : "Billing is unavailable."
      )
      setBusy(false)
    }
  }
  async function deleteAccount(event: SubmitEvent<HTMLFormElement>) {
    event.preventDefault()
    const password = String(
      new FormData(event.currentTarget).get("password") ?? ""
    )
    setBusy(true)
    setMessage("")
    try {
      const result = await client.deleteUser({
        password,
        callbackURL: `${window.location.origin}/account/`,
      })
      if (result.error) throw new Error(result.error.message)
      setUsage(null)
      setMessage("Your account has been deleted.")
    } catch (error) {
      setMessage(
        error instanceof Error
          ? error.message
          : "Could not delete your account. Please try again."
      )
    } finally {
      setBusy(false)
    }
  }
  const title =
    mode === "register"
      ? "Create your account"
      : mode === "forgot"
        ? "Reset your password"
        : mode === "reset"
          ? "Choose a new password"
          : "Welcome back"
  return (
    <div className="mx-auto flex max-w-lg flex-col gap-8 px-5 py-16">
      <div className="flex flex-col gap-3">
        <p className="text-sm text-muted-foreground">
          Neden account{sandbox ? " · Test environment" : ""}
        </p>
        <h1 className="text-3xl font-semibold tracking-tight">
          {session ? "Your account" : title}
        </h1>
        <p className="text-muted-foreground">
          The desktop player is free. An account is for the optional hosted AI
          service.
        </p>
      </div>
      {(message || sessionError) && (
        <Alert role="status">
          <AlertDescription>
            {message ||
              "Unable to check your session. Please refresh to try again."}
          </AlertDescription>
        </Alert>
      )}
      {isPending ? (
        <p role="status">Checking your session…</p>
      ) : session ? (
        <>
          <div className="flex items-center justify-between gap-4">
            <p className="min-w-0 break-all">{session.user.email}</p>
            <Button
              variant="ghost"
              disabled={busy}
              onClick={async () => {
                setBusy(true)
                const result = await client.signOut()
                setBusy(false)
                if (result.error)
                  setMessage("Could not sign out. Please try again.")
              }}
            >
              Sign out
            </Button>
          </div>
          <Separator />
          <div className="flex flex-col gap-4">
            <h2 className="text-xl font-semibold">Hosted AI</h2>
            {!billingAvailable && (
              <p className="text-muted-foreground">
                The hosted service is being tested. Subscriptions are not open
                yet.
              </p>
            )}
            {usage?.subscribed ? (
              <>
                <p>
                  {usage.words.used} of {usage.words.limit} word lookups used
                </p>
                <p>
                  {usage.explanations.used} of {usage.explanations.limit}{" "}
                  explanations and follow-ups used
                </p>
                {usage.periodEnd && (
                  <p className="text-sm text-muted-foreground">
                    {usage.cancelAtPeriodEnd
                      ? "Access ends"
                      : "Allowance renews"}{" "}
                    {new Date(usage.periodEnd * 1000).toLocaleDateString()}
                  </p>
                )}
              </>
            ) : (
              billingAvailable && (
                <>
                  <p>
                    $10/month · 1,000 word lookups and 300 explanations or
                    follow-ups.
                  </p>
                  <Button
                    disabled={busy}
                    onClick={() => openBilling("checkout")}
                  >
                    {sandbox ? "Try sandbox checkout" : "Subscribe"}
                  </Button>
                </>
              )
            )}
            {usage?.hasBillingAccount && (
              <Button
                variant="outline"
                disabled={busy}
                onClick={() => openBilling("portal")}
              >
                Manage billing
              </Button>
            )}
          </div>
          <Separator />
          <details>
            <summary className="text-sm text-muted-foreground">
              Delete account
            </summary>
            <form onSubmit={deleteAccount} className="mt-5">
              <FieldGroup>
                <p className="text-sm text-muted-foreground">
                  This cancels your subscription immediately and removes your
                  account and usage records. Your local videos and flashcards
                  stay on your device. Stripe retains billing records.
                </p>
                <Field>
                  <FieldLabel htmlFor="delete-password">
                    Confirm with your password
                  </FieldLabel>
                  <Input
                    id="delete-password"
                    name="password"
                    type="password"
                    autoComplete="current-password"
                    required
                    disabled={busy}
                  />
                </Field>
                <Button variant="destructive" type="submit" disabled={busy}>
                  Permanently delete account
                </Button>
              </FieldGroup>
            </form>
          </details>
        </>
      ) : (
        <>
          <form onSubmit={submit}>
            <FieldGroup>
              {mode === "register" && (
                <Field>
                  <FieldLabel htmlFor="name">Name</FieldLabel>
                  <Input
                    id="name"
                    name="name"
                    autoComplete="name"
                    required
                    maxLength={100}
                    disabled={busy}
                  />
                </Field>
              )}
              {mode !== "reset" && (
                <Field>
                  <FieldLabel htmlFor="email">Email</FieldLabel>
                  <Input
                    id="email"
                    name="email"
                    type="email"
                    autoComplete="email"
                    required
                    disabled={busy}
                  />
                </Field>
              )}
              {mode !== "forgot" && (
                <Field>
                  <FieldLabel htmlFor="password">Password</FieldLabel>
                  <Input
                    id="password"
                    name="password"
                    type="password"
                    autoComplete={
                      mode === "sign-in" ? "current-password" : "new-password"
                    }
                    minLength={mode === "sign-in" ? undefined : 12}
                    maxLength={128}
                    required
                    disabled={busy}
                  />
                  {mode !== "sign-in" && (
                    <FieldDescription>
                      Use at least 12 characters.
                    </FieldDescription>
                  )}
                </Field>
              )}
              <Button type="submit" disabled={busy}>
                {busy
                  ? "Please wait…"
                  : mode === "register"
                    ? "Create account"
                    : mode === "forgot"
                      ? "Send reset link"
                      : mode === "reset"
                        ? "Update password"
                        : "Sign in"}
              </Button>
            </FieldGroup>
          </form>
          <div className="flex flex-wrap gap-2">
            <Button
              variant="ghost"
              disabled={busy}
              onClick={() => {
                setMode(mode === "sign-in" ? "register" : "sign-in")
                setMessage("")
              }}
            >
              {mode === "sign-in" ? "Create an account" : "Back to sign in"}
            </Button>
            {mode === "sign-in" && (
              <Button
                variant="ghost"
                disabled={busy}
                onClick={() => {
                  setMode("forgot")
                  setMessage("")
                }}
              >
                Forgot password?
              </Button>
            )}
          </div>
        </>
      )}
    </div>
  )
}
